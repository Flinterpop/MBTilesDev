

#include "sqlite3.h"
#include <vector>
#include <string>
#include <d3d11.h>


static ID3D11Device* g_pd3dDevice;


#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <imgui.h>  //for LoadTextureFromMemory

// Simple helper function to load an image into a DX11 texture with common settings
bool LoadTextureFromMemory(const void* data, size_t data_size, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}


ID3D11ShaderResourceView* get_MBTile(size_t zoom, size_t  tile_column, size_t tile_row);


static int get_tables(const std::string& file_name);
static int get_metadata();



static std::string MBTilesFileName{};
static sqlite3* m_db{};
static std::vector<std::string> m_tables{};

bool SetMBTilesDataBaseByFileName(const std::string& file_name, ID3D11Device*d)
{
    g_pd3dDevice = d;
    MBTilesFileName = file_name;
    int retVal = get_tables(MBTilesFileName);
    if (SQLITE_ERROR == retVal)
    {
        printf("Failed to find any tables in %s\r\n", MBTilesFileName.c_str());
        return true;//true means fail
    }
    retVal = get_metadata();
    if (SQLITE_ERROR == retVal)
    {
        puts("get_metadata failed");
        return true;
    }

}

static int get_tables(const std::string& file_name)
{
    sqlite3_stmt* stmt = 0;

    if (sqlite3_open_v2(file_name.c_str(), &m_db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_tables SQLITE_ERROR 1");
        return SQLITE_ERROR;
    }

    if (sqlite3_prepare_v2(m_db, "SELECT name FROM sqlite_master WHERE type='table';", -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_tables SQLITE_ERROR 2");
        return SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::string table = (const char*)sqlite3_column_text(stmt, 0);
        m_tables.push_back(table);
    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

static int get_metadata()
{
    sqlite3_stmt* stmt = 0;

    if (sqlite3_prepare_v2(m_db, "SELECT * FROM metadata;", -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_metadata SQLITE_ERROR");
        return SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::string row_name = (const char*)sqlite3_column_text(stmt, 0); //field 'name '
        std::string row_value = (const char*)sqlite3_column_text(stmt, 1); //field 'value '
        printf("%s | %s\r\n", row_name.c_str(), row_value.c_str());

    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}



ID3D11ShaderResourceView *get_MBTile(size_t zoom, size_t  tile_column, size_t tile_row)
{
    ID3D11ShaderResourceView* texture{};
    sqlite3_stmt* stmt = 0;
    std::string sql = "SELECT tile_data FROM tiles WHERE ";
    sql += "zoom_level=";
    sql += std::to_string(zoom);
    sql += " AND tile_column=";
    sql += std::to_string(tile_column);
    sql += " AND tile_row=";
    sql += std::to_string(tile_row);
    sql += ";";

    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        return nullptr; // SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int col = 0;
        int size_blob = sqlite3_column_bytes(stmt, col);
        const void* blob = sqlite3_column_blob(stmt, col);
        printf("sizing: blob is size: %5d\t\n", size_blob);
        
        if (size_blob > 100)
        {
            int image_width;
            int image_height;
            bool retVal = LoadTextureFromMemory((const void*)blob, size_blob, &texture, &image_width, &image_height);
            if (false == retVal)
            { 
                puts("Fail in LoadTextureFromMemory");
                return nullptr;
            }
        }
    }

    sqlite3_finalize(stmt);
    //return SQLITE_OK;
    return texture;
}



