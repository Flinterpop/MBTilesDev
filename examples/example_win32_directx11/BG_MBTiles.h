#pragma once
#include <string>
#include <d3d11.h>


bool SetMBTilesDataBaseByFileName(const std::string& file_name, ID3D11Device* d);
ID3D11ShaderResourceView* get_MBTile(size_t zoom, size_t  tile_column, size_t tile_row);

