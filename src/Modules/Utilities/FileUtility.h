#pragma once

#include <codecvt>
#include <locale>
#include <string>
#include <fstream>

inline std::wstring utf8_to_wstring(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

inline std::string wstring_to_utf8(const std::wstring &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

inline std::wstring GetAssetFullPath(const std::wstring& assetName)
{
    std::wstring path = utf8_to_wstring(ASSETS_PATH) + assetName;
    if (!std::ifstream(wstring_to_utf8(path)).good())
        path = L"assets/" + assetName;
    return path;
}

inline std::string GetAssetFullPath(const std::string& assetName)
{
    return wstring_to_utf8(GetAssetFullPath(utf8_to_wstring(assetName)));
}
