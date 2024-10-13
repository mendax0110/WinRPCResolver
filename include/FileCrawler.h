#ifndef FILE_CRAWLER_H
#define FILE_CRAWLER_H

#include <string>
#include <vector>
#include <windows.h>

/// @brief FileCrawler class to search for files with specific extensions \class FileCrawler
class FileCrawler
{
public:
    FileCrawler(const std::string& rootDir);

    /*!
     * @brief Find files with specific extensions
     * @param extensions The file extensions to search for
     * @return std::vector<std::string> The list of files found
     * */
    std::vector<std::string> findFiles(const std::vector<std::string>& extensions);

    /*!
     * @brief Check if a file is related to RPC 
     * @param filePath The file path
     * @return bool True if the file is related to RPC, false otherwise
     */
    bool isRpcRelatedFile(const std::string& filePath);

    /*!
     * @brief Check if the file's executable service path matches the file path
     * @param filePath The file path
     * @return bool True if the executable service path matches the file path, false otherwise
     */
    bool checkExecutableServicePath(const std::string& filePath);

    /*!
     * @brief Check if a string ends with a specific suffix
     * @param str The string to check
     * @param suffix The suffix to check for
     * @return bool True if the string ends with the suffix, false otherwise
     */
    bool endsWith(const std::string& str, const std::string& suffix);

    /*!
     * @brief Get the error message for a specific error code
     * @param errorCode The error code
     * @return std::string The error message
     */
    std::string getErrorMessage(DWORD errorCode);

private:
    std::string m_rootDir;

    /*!
     * @brief Search a directory for files with specific extensions
     * @param dir The directory to search
     * @param extensions The file extensions to search for
     * @param foundFiles The list of files found
     * */
    void searchDirectory(const std::string& dir, const std::vector<std::string>& extensions, std::vector<std::string>& foundFiles);
};

#endif // FILE_CRAWLER_H
