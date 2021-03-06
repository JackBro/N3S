#include "stdafx.h"
#include "GameData.hpp"

RomInfo getRomInfo(char * data)
{
	RomInfo r;
	r.prgSize = *(data + 4);
	r.chrSize = *(data + 5);
	return r;
}

// Generate SpriteMesh from CHR data in a ROM
SpriteMesh::SpriteMesh(char* spriteData)
{
	voxels.reset(new VoxelCollection());
	voxels->clear();
	int voxelStart = spriteSize / 2;
	for (int i = 0; i < 8; i++)
	{
		setVoxelColors(*(spriteData + i), *(spriteData + i + 8), &voxels->voxels[voxelStart + (i * 8)]);
	}
	meshExists = buildMesh();
}

SpriteMesh::SpriteMesh(json j)
{
	// Grab ID
	id = stoi(j["id"].get<string>());
	// Load voxels
	voxels.reset(new VoxelCollection(j["voxels"]));
	// Build mesh
	meshExists = buildMesh();
	// If we're not in editor mode, don't keep the voxel array loaded
	// voxels.release();
}

void SpriteMesh::setVoxel(int x, int y, int z, int color)
{
	// Calculate which slot in 'flattened' array this pertains to
	int voxelSlot = x + (y * spriteWidth) + (z * (spriteHeight * spriteWidth));
	if (voxelSlot >= 0 && voxelSlot < spriteSize) 
	{
		voxels->voxels[voxelSlot].color = color;
	}
}

void SpriteMesh::buildZMeshes()
{
	int i = 0;
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			int zArray[32];
			for (int z = 0; z < 32; z++)
			{
				zArray[z] = voxels->getVoxel(x, y, z).color;
			}
			zMeshes[i] = GameData::getSharedMesh(zArray);
			i++;
		}
	}
}

VoxelMesh buildZMesh(int zArray[32])
{
	VoxelMesh mesh = VoxelMesh();
	vector<ColorVertex> vertices;
	int sideCount = 0;
	for (int z = 0; z < spriteDepth; z++)
	{
		int color = zArray[z];
		// See if the voxel is not empty
		if (color > 0)
		{
			// Draw left, right, top, and bottom edges, which should be visible on Z-Meshes no matter what
			buildSide(&vertices, 0, 0, z, color, VoxelSide::left);
			buildSide(&vertices, 0, 0, z, color, VoxelSide::right);
			buildSide(&vertices, 0, 0, z, color, VoxelSide::top);
			buildSide(&vertices, 0, 0, z, color, VoxelSide::bottom);
			sideCount += 4;
			// See if front and/or back are exposed, and add sides if so
			if (z == 0 || zArray[z - 1] == 0)
			{
				buildSide(&vertices, 0, 0, z, color, front);
				sideCount++;
			}
			if (z == 31 || zArray[z + 1] == 0)
			{
				buildSide(&vertices, 0, 0, z, color, back);
				sideCount++;
			}
			// TODO: Also add > evaluation for semi-transparent voxels in the future
		}
	}
	mesh.size = sideCount * 6;
	mesh.type = color;
	mesh.buffer = N3s3d::createBufferFromColorVerticesV(&vertices, mesh.size);
	return mesh;
}

void buildSide(vector<ColorVertex> * vertices, int x, int y, int z, int color, VoxelSide side) {
	// Translate voxel-space xyz into model-space based on pixel size in 3d space, phew
	float xf = x * pixelSizeW;
	float yf = y * -pixelSizeH;
	float zf = z * pixelSizeW;
	// Init 4 vertices
	ColorVertex v1, v2, v3, v4;
	// Set all colors to greyscale
	if (color == 1)
	{
		v4.Col = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		v1.Col = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		v2.Col = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		v3.Col = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else if (color == 2)
	{
		v1.Col = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
		v2.Col = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
		v3.Col = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
		v4.Col = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	}
	else if (color == 3)
	{
		v1.Col = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
		v2.Col = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
		v3.Col = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
		v4.Col = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	}

	// Switch based on side
	switch (side) {
	case VoxelSide::left:
		v1.Pos = XMFLOAT4(xf, yf, zf + pixelSizeW, 1.0f);
		v2.Pos = XMFLOAT4(xf, yf, zf, 1.0f);
		v3.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf, 1.0f);
		v4.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		break;
	case VoxelSide::right:
		v1.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf, 1.0f);
		v2.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf + pixelSizeW, 1.0f);
		v3.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		v4.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf, 1.0f);
		break;
	case VoxelSide::top:
		v1.Pos = XMFLOAT4(xf, yf, zf + pixelSizeW, 1.0f);
		v2.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf + pixelSizeW, 1.0f);
		v3.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf, 1.0f);
		v4.Pos = XMFLOAT4(xf, yf, zf, 1.0f);
		break;
	case VoxelSide::bottom:
		v1.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf, 1.0f);
		v2.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf, 1.0f);
		v3.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		v4.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		break;
	case VoxelSide::front:
		v1.Pos = XMFLOAT4(xf, yf, zf, 1.0f);
		v2.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf, 1.0f);
		v3.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf, 1.0f);
		v4.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf, 1.0f);
		break;
	case VoxelSide::back:
		v1.Pos = XMFLOAT4(xf + pixelSizeW, yf, zf + pixelSizeW, 1.0f);
		v2.Pos = XMFLOAT4(xf, yf, zf + pixelSizeW, 1.0f);
		v3.Pos = XMFLOAT4(xf, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		v4.Pos = XMFLOAT4(xf + pixelSizeW, yf - pixelSizeH, zf + pixelSizeW, 1.0f);
		break;
	}
	vertices->push_back(v1);
	vertices->push_back(v2);
	vertices->push_back(v4);
	vertices->push_back(v2);
	vertices->push_back(v3);
	vertices->push_back(v4);
}

unordered_map<string, SharedMesh> GameData::sharedMeshes;

GameData::GameData(char * data)
{
	// Get ROM metadata
	romInfo = getRomInfo(data);
	// Grab char data, which is offset by 16 bytes due to iNES header
	char* chrData = data + 16 + (romInfo.prgSize * 16384);
	totalSprites = romInfo.chrSize * 512;
	bool ignoreDuplicateSprites = true; // Temporary, this will eventually get specified by user
	// Create maps to ignore duplicate sprites
	unordered_set<string> previousSprites;
	int spritesIgnored = 0;
	// Loop through and create all static sprites
	for (int i = 0; i < totalSprites; i++)
	{
		// Build a string that contains the 16 bytes of current sprite
		char* current = chrData + (i * 16);
		string spriteData = "";
		for (int i = 0; i < 16; i++)
		{
			spriteData += *(current + i);
		}
		// Check for duplicate
		if (previousSprites.count(spriteData) < 1)
		{
			int uniqueCount = i - spritesIgnored;
			// Add the data of the sprite we're going to generate to the map of duplicates
			previousSprites.insert(spriteData);
			// Build mesh and add to list
			shared_ptr<SpriteMesh> mesh = make_shared<SpriteMesh>(current);
			mesh->id = uniqueCount;
			meshes[uniqueCount] = mesh;
			// Build the static sprite
			shared_ptr<VirtualSprite> vSprite = make_shared<VirtualSprite>(spriteData, meshes[uniqueCount]);
			vSprite->appearancesInRomChr.push_back(i);	// Record where this sprite occured in CHR data
			vSprite->id = uniqueCount;
			sprites[i] = vSprite;
			spritesByChrData[spriteData] = vSprite;
		}
		else
		{
			// Record appreance in CHR data but don't bother building new sprite/mesh
			spritesByChrData[spriteData]->appearancesInRomChr.push_back(i);
			spritesIgnored++;
		}
	}
}

GameData::GameData(char* data, json j)
{
	// Get ROM metadata
	romInfo = getRomInfo(data);
	// Load meshes
	for each (json jM in j["meshes"])
	{
		// Get ID from (probably padded) string
		const string s = jM["id"];
		int id = stoi(s);
		// Build mesh from JSON and add to map of meshes by ID
		shared_ptr<SpriteMesh> mesh = make_shared<SpriteMesh>(jM);
		mesh->id = id;
		meshes[id] = mesh;
	}
	// Load VirtualSprites
	for each (json jS in j["sprites"])
	{
		shared_ptr<VirtualSprite> sprite = make_shared<VirtualSprite>(jS, meshes);
		sprites[sprite->id] = sprite;
		spritesByChrData[sprite->chrData] = sprite;
	}
}

shared_ptr<VirtualSprite> GameData::getSpriteByChrData(char * data)
{
	// Get string of CHR data
	string s = "";
	for (int i = 0; i < 16; i++)
	{
		s += *(data + i);
	}
	// Return from sprites if it exists, otherwise return sprite 0 :(
	if (spritesByChrData.count(s) > 0)
		return spritesByChrData[s];
	else
		return sprites[0];
}

VoxelMesh GameData::getSharedMesh(int zArray[32])
{
	// Create simple string hash from the voxels
	string hash = "";
	char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
	for (int z = 0; z < 31; z++)
	{
		hash += digits[zArray[z]];
		hash += '.';
	}
	hash += digits[zArray[31]];
	// Return mesh if it exists, otherwise create
	if (sharedMeshes.count(hash) > 0)
	{
		// Increase number of references
		sharedMeshes[hash].referenceCount++;
		// Return reference
		return sharedMeshes[hash].mesh;
	}
	else
	{
		SharedMesh m = SharedMesh();
		m.mesh = buildZMesh(zArray);
		m.referenceCount++;
		sharedMeshes[hash] = m;
		return m.mesh;
	}
}

void GameData::releaseSharedMesh(string hash)
{
	// Make sure this actually exists
	if (sharedMeshes.count(hash) > 0)
	{
		sharedMeshes[hash].referenceCount--;
		// Delete this mesh if it is no longer in use
		if (sharedMeshes[hash].referenceCount <= 0)
		{
			sharedMeshes[hash].mesh.buffer->Release();
			sharedMeshes.erase(hash);
		}
	}
}

void GameData::unload()
{
	// Release all full meshes
	for (auto kvp : meshes)
	{
		if (kvp.second->meshExists)
		{
			kvp.second->mesh.buffer->Release();
		}
	}
	// Release all partial meshes
	for (auto kv : sharedMeshes)
	{
		if (kv.second.mesh.buffer != nullptr)
			kv.second.mesh.buffer->Release();
	}
	meshes.clear();
	sprites.clear();
	sharedMeshes.clear();
}

string GameData::getJSON()
{
	json j;
	j["info"]["prgSize"] = romInfo.prgSize;
	j["info"]["chrSize"] = romInfo.chrSize;
	j["info"]["mapper"] = romInfo.mapper;
	j["info"]["trainer"] = romInfo.trainer;
	j["info"]["playChoice10"] = romInfo.playChoice10;
	j["info"]["PAL;"] = romInfo.PAL;
	for each (auto kvp in meshes)
	{
		// Create string from ID, pad to 5 digits to enforce proper order
		j["meshes"].push_back(kvp.second->getJSON());
	}
	for each (auto kvp in sprites)
	{
		j["sprites"].push_back(kvp.second->getJSON());
	}
	return j.dump(4);
}

bool SpriteMesh::buildMesh()
{
	vector<ColorVertex> vertices;
	int sideCount = 0;
	for (int z = 0; z < spriteDepth; z++)
	{
		for (int y = 0; y < spriteHeight; y++)
		{
			for (int x = 0; x < spriteWidth; x++)
			{
				int color = voxels->getVoxel(x, y, z).color;
				// See if the voxel is not empty
				if (color > 0)
				{
					// Check each adjacent voxel/edge and draw a square as needed
					// TODO: Also add > evaluation for semi-transparent voxels in the future?
					if (x == 0 || voxels->getVoxel(x - 1, y, z).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::left);
						sideCount++;
					}
					if (x == spriteWidth - 1 || voxels->getVoxel(x + 1, y, z).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::right);
						sideCount++;
					}
					if (y == 0 || voxels->getVoxel(x, y - 1, z).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::top);
						sideCount++;
					}
					if (y == spriteHeight - 1 || voxels->getVoxel(x, y + 1, z).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::bottom);
						sideCount++;
					}
					if (z == 0 || voxels->getVoxel(x - 1, y, z - 1).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::front);
						sideCount++;
					}
					if (z == spriteDepth - 1 || voxels->getVoxel(x, y, z + 1).color == 0) {
						buildSide(&vertices, x, y, z, color, VoxelSide::back);
						sideCount++;
					}
				}
			}
		}
	}
	mesh.size = sideCount * 6;
	mesh.type = color;
	// Return true if there is an actual mesh to render
	if (vertices.size() > 0)
	{
		mesh.buffer = N3s3d::createBufferFromColorVerticesV(&vertices, mesh.size);
		buildZMeshes();
		return true;
	}
	else
	{
		return false;
	}
}

VoxelCollection::VoxelCollection()
{
	clear();
}

VoxelCollection::VoxelCollection(char * sprite)
{
	clear();
	int spriteGraphic[64];
	int offset = 16;
	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			// setVoxel(x, y,)
		}
	}
}

VoxelCollection::VoxelCollection(json j)
{
	for (int z = 0; z < 32; z++)
	{
		for (int y = 0; y < 8; y++)
		{
			string yString = j[y];
			for (int x = 0; x < 8; x++)
			{
				int8_t color = yString[z * 9 + x] - 48;
				setVoxel(x, y, z, { color });
			}
		}
	}
}

Voxel VoxelCollection::getVoxel(int x, int y, int z)
{
	int voxelSlot = x + (y * spriteWidth) + (z * (spriteHeight * spriteWidth));
	return voxels[voxelSlot];
}

void VoxelCollection::setVoxel(int x, int y, int z, Voxel v)
{
	int voxelSlot = x + (y * spriteWidth) + (z * (spriteHeight * spriteWidth));
	voxels[voxelSlot] = v;
}

void VoxelCollection::clear()
{
	for (int i = 0; i < spriteSize; i++)
	{
		voxels[i].color = 0;
	}
}

json VoxelCollection::getJSON()
{
	json j;
	string s[8] = { "", "", "", "", "", "", "", "" };
	// The data is staggered in a weird way
	// I want a json string for each row, where the sprite's 8 y-axis layers are on top of eachother the same way in text
	for (int z = 0; z < 32; z++)
	{
		for (int y = 0; y < 8; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				s[y] += intChars[getVoxel(x, y, z).color];
			}
			s[y] += '|'; // Horizontal divider
		}
	}
	for (int y = 0; y < 8; y++)
	{
		s[y].pop_back();	// Trim last | delimiter
		j.push_back(s[y]);	// Push string to json structure
	}
	return j;
}

// Set the color of 8 consecutive voxels using CHR sprite data
void setVoxelColors(char a, char b, Voxel* row)
{
	for (int i = 0; i < 8; i++)
	{
		int color = 0;
		bool bit1 = getBitLeftSide(a, i);
		bool bit2 = getBitLeftSide(b, i);
		if (bit1)
			color += 1;
		if (bit2)
			color += 2;
		(row + i)->color = color;
	}
}

bool getBitLeftSide(char byte, int position)
{
	return (byte << position) & 0x80;
}

string getPaddedStringFromInt(int value, int length)
{
	string s;
	for (int i = to_string(value).length(); i < length; i++)
	{
		s += '0';
	}
	s.append(to_string(value));
	return s;
}

int charToInt(char c)
{
	// Thanks Niels: http://stackoverflow.com/questions/17261798/converting-a-hex-string-to-a-byte-array
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0;
}

//VirtualSprite::VirtualSprite()
//{
//}

VirtualSprite::VirtualSprite(string chrData, shared_ptr<SpriteMesh> defaultMesh) : chrData(chrData), defaultMesh(defaultMesh)
{
}

VirtualSprite::VirtualSprite(json j, map<int, shared_ptr<SpriteMesh>> meshes)
{
	// Grab the ID from padded string int
	id = stoi(j["id"].get<string>());
	// Get the CHR data (convert from hex string to byte/char string)
	getChrStringFromText(j["chrData"]);
	// Snag default mesh from passed map
	int mesh = j["defaultMesh"];
	defaultMesh = meshes[mesh];
	// Get other values
	for each (int i in j["appearancesInRomChr"])
	{
		appearancesInRomChr.push_back(i);
	}
	description = j["description"].get<string>();
}

void VirtualSprite::renderOAM(shared_ptr<PpuSnapshot> snapshot, int x, int y, int palette, bool mirrorH, bool mirrorV, Crop crop)
{
	// TODO: Check dynamic stuff
	defaultMesh->render(x, y, palette, mirrorH, mirrorV, crop);
}

void VirtualSprite::renderNametable(shared_ptr<PpuSnapshot> snapshot, int x, int y, int palette, int nametableX, int nametableY, Crop crop)
{
	// TODO: Check dynamic stuff
	defaultMesh->render(x, y, palette, false, false, crop);
}

json VirtualSprite::getJSON()
{
	json j;
	j["id"] = getPaddedStringFromInt(id, 5);
	j["chrData"] = serializeChrDataAsText();
	j["appearancesInRomChr"] = appearancesInRomChr;
	j["description"] = description;
	j["defaultMesh"] = defaultMesh->id;
	return j;
}

string VirtualSprite::serializeChrDataAsText()
{
	string s = "";
	for (int i = 0; i < 16; i++)
	{
		unsigned char c = chrData[i];
		int a = (int)c >> 4;
		int b = (int)c & 15;
		s += hexCodes[a];
		s += hexCodes[b];
	}
	return s;
}

void VirtualSprite::getChrStringFromText(string s)
{
	for (int i = 0; i < 16; i++)
	{
		int position = i * 2;
		// Grab both characters for each value
		int a = charToInt(s[position]);
		int b = charToInt(s[position + 1]);
		// Assuming "a" is leftmost 4 bits, multiply by 16 and add B to get original "byte"
		chrData += ((a * 16) + b);
	}
}

void SpriteMesh::render(int x, int y, int palette, bool mirrorH, bool mirrorV, Crop crop)
{
	if (meshExists)
	{
		// Check if we're cropping
		if (crop.zeroed())
		{
			N3s3d::selectPalette(palette);
			float posX, posY;
			posX = -1.0f + (pixelSizeW * x);
			posY = 1.0f - (pixelSizeH * y);
			N3s3d::updateMirroring(mirrorH, mirrorV);
			if (mirrorH)
				posX += (pixelSizeW * 8);
			if (mirrorV)
				posY -= (pixelSizeH * 8);
			N3s3d::updateWorldMatrix(posX, posY, 0);
			N3s3d::renderMesh(&mesh);
		}
		else
		{
			// Render the cropped version with zMeshes
			N3s3d::selectPalette(palette);
			int height = 8 - crop.top - crop.bottom;
			int width = 8 - crop.left - crop.right;
			for (int posY = 0; posY < height; posY++)
			{
				for (int posX = 0; posX < width; posX++)
				{
					// Grab different zMeshes based on mirroring
					int meshX, meshY;
					if (mirrorH)
						meshX = 7 - (posX + crop.left);
					else
						meshX = posX + crop.left;
					if (mirrorV)
						meshY = 7 - (posY + crop.top);
					else
						meshY = posY + crop.top;
					if (zMeshes[(meshY * 8) + meshX].buffer != nullptr)
					{
						N3s3d::updateWorldMatrix(-1.0f + ((x + posX) * pixelSizeW), 1.0f - ((y + posY) * pixelSizeH), 0);
						N3s3d::renderMesh(&zMeshes[(meshY * 8) + meshX]);
					}
				}
			}
		}

	}
}

json SpriteMesh::getJSON()
{
	json j;
	j["id"] = getPaddedStringFromInt(id, 5);
	j["voxels"] = voxels->getJSON();
	return j;
}
