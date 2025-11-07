#pragma once
#include <string>
#include <vector>
#include <d3d11.h>


bool SetMBTilesDataBaseByFileName(const std::string& file_name, ID3D11Device* d);
ID3D11ShaderResourceView* get_MBTile(size_t zoom, size_t  tile_column, size_t tile_row);


int get_metadata();
int get_MBTileName(std::string& retVal);
int get_NumTiles(int &retVal);

// Structure to hold the parsed bounds
struct Bounds {
    double left;
    double bottom;
    double right;
    double top;
};



Bounds GetMBTilesBounds();

//DTED

bool bg_CreateDTED0Table();
std::vector<char> readFileAsBlob(std::string& filePath);
bool insertDTEDBlob(std::vector<char> blob);
