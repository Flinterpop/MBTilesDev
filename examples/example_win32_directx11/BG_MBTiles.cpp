

#include "sqlite3.h"
#include <vector>
#include <string>
#include <sstream>
#include <d3d11.h>

#include "BG_MBTiles.h"

static ID3D11Device* g_pd3dDevice;


#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <imgui.h>  //for LoadTextureFromMemory
#include <iostream>

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
int get_metadata();

static int get_tables(const std::string& file_name);



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

int get_metadata()
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

int get_MBTileName(std::string &retVal)
{
    retVal = "error";
    sqlite3_stmt* stmt = 0;

    if (sqlite3_prepare_v2(m_db, "SELECT * FROM \"main\".\"metadata\" where (\"name\") = ('name');", -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_metadata SQLITE_ERROR");
        retVal = "get_metadata SQLITE_ERROR";
        return SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) //should only happen once
    {
        //std::string row_name = (const char*)sqlite3_column_text(stmt, 0); //field 'name '
        std::string row_value = (const char*)sqlite3_column_text(stmt, 1); //field 'value '
        printf("MBTile fille name is %s\r\n", row_value.c_str());
        retVal = row_value;
    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

int get_MBTileBounds(double &left, double& bottom, double& right, double& top)
{
    sqlite3_stmt* stmt = 0;

    if (sqlite3_prepare_v2(m_db, "SELECT * FROM \"main\".\"metadata\" where (\"name\") = ('bounds');", -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_metadata SQLITE_ERROR");
        return SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {

        std::string row_value = (const char*)sqlite3_column_text(stmt, 1); //field 'value '
        printf("bounds are %s\r\n", row_value.c_str());


        double val = sqlite3_column_double(stmt, 1);
        printf("MBTile bounds are %f\r\n", val);
        val = sqlite3_column_double(stmt, 2);
        printf("MBTile bounds are %f\r\n", val);

        
        


    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}


int get_NumTiles(int &retVal)
{
    sqlite3_stmt* stmt = 0;

    if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM tiles;", -1, &stmt, 0) != SQLITE_OK)
    {
        sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        puts("get_metadata SQLITE_ERROR");
        return SQLITE_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) //should only happen once
    {
        retVal = sqlite3_column_int(stmt, 0);
        printf("Number of tiles %d\r\n", retVal);
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





// Function to parse the comma-separated bounds string
static Bounds parseBoundsString(const std::string& boundsStr) {
    std::stringstream ss(boundsStr);
    std::string segment;
    std::vector<double> values;

    while (std::getline(ss, segment, ',')) {
        try {
            values.push_back(std::stod(segment));
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Error parsing bounds value: " + std::string(e.what()));
        }
    }

    if (values.size() != 4) {
        throw std::runtime_error("Bounds string does not contain 4 values: " + boundsStr);
    }

    return { values[0], values[1], values[2], values[3] };
}



// Function to retrieve the bounds string from the SQLite metadata table
static std::string getBoundsStringFromMBTiles()
{
    std::string boundsStr = "";
    const char* sql = "SELECT value FROM metadata WHERE name = 'bounds';";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                boundsStr = std::string(reinterpret_cast<const char*>(value));
            }
        }
    }
    else {
        std::cerr << "Failed to execute query: " << sqlite3_errmsg(m_db) << std::endl;
    }
    sqlite3_finalize(stmt);
    return boundsStr;
}


Bounds GetMBTilesBounds()
{
    Bounds bounds{};
    std::string boundsStr = getBoundsStringFromMBTiles();
    if (!boundsStr.empty()) bounds = parseBoundsString(boundsStr);
    else std::cout << "Bounds not found in metadata." << std::endl;
    return bounds;
}



////DTED


//Create and intialize Application Database with 3 tables
bool bg_CreateDTED0Table()
{
    std::string AppIni_dbName = "DTED0Blob.db";

    sqlite3* db;
    char* zErrMsg = 0;
    int rc = sqlite3_open(AppIni_dbName.c_str(), &db);
    if (rc)
    {
        printf("\tCan't open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    char* errorMsg = nullptr;

    //Create Application  INT table

    for (int e = 0;e < 179;e++)
    {

        char buf[10];
        sprintf(buf, "w%03d", e);
        std::string b = buf;

        std::string sql =
            "CREATE TABLE " + b + " ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            //"ns TEXT NOT NULL, "
            "dted BLOB NOT NULL"
            ");";

        if (sqlite3_exec(db, sql.c_str(), 0, 0, &errorMsg) != SQLITE_OK)
        {
            printf("\tSQLite error: %s\r\n", errorMsg);
            sqlite3_free(errorMsg);
            return false;
        }
        else printf("\tCreated east Table\r\n");

    }


    printf("\tCreated DB: %s\r\n", AppIni_dbName.c_str());

    return true;
}


bool insertDTEDBlob(std::vector<char> blob)
{
    std::string AppIni_dbName = "DTED0Blob.db";

    sqlite3* db;
    char* zErrMsg = 0;
    int rc = sqlite3_open(AppIni_dbName.c_str(), &db);
    if (rc)
    {
        printf("\tCan't open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }


    // Prepare statement
    sqlite3_stmt* stmt;
    const char* insert_sql = "INSERT INTO w104 (dted) VALUES (?);";

    //"INSERT INTO images (name, data) VALUES (?, ?);";

    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
    //int rc = sqlite3_prepare_v2(db, insert_sql, -1, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        //sqlite3_close(db);
        return 1;
    }

    // Bind BLOB data
    rc = sqlite3_bind_blob(stmt, 1, blob.data(), static_cast<int>(blob.size()), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind blob: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        //sqlite3_close(db);
        return 1;
    }

    // Execute statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
    }
    else {
        std::cout << "BLOB inserted successfully." << std::endl;
    }

    // Finalize statement and close database
    sqlite3_finalize(stmt);
    //sqlite3_close(m_db);


}



#include <fstream>


// Function to read a file into a vector of characters (blob)
std::vector<char> readFileAsBlob(std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate); // Open in binary mode and go to end
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return {}; // Return empty vector on error
    }

    std::streamsize size = file.tellg(); // Get file size
    printf("File %s is size %d\r\n", filePath.c_str(), (int)size);
    file.seekg(0, std::ios::beg); // Go back to beginning

    std::vector<char> buffer(size); // Create buffer of appropriate size
    if (!file.read(buffer.data(), size)) { // Read entire file into buffer
        std::cerr << "Error: Could not read file " << filePath << std::endl;
        return {}; // Return empty vector on error
    }

    std::cout << "File read successfully. Size: " << size << " bytes." << std::endl;

    return buffer;
}

