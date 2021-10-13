#include "serverconnection.h"
#include <boost/filesystem.hpp>
#include "ftpserver.h"
#include <boost/filesystem.hpp>

//! Деструктор
serverconnection::~serverconnection() {
    std::cout << "Connection terminated to client (connection id " << this->connectionId << ")" << std::endl;
    delete this->fo;
    close(this->fd);
    this->directories.clear();
    this->files.clear();
}

//! Конструктор 
serverconnection::serverconnection(int filedescriptor, unsigned int connId, std::string defaultDir, std::string hostId, unsigned short commandOffset) : fd(filedescriptor), connectionId(connId), dir(defaultDir), hostAddress(hostId), commandOffset(commandOffset), closureRequested(false), uploadCommand(false), downloadCommand(false),  receivedPart(0), parameter("") {
//    this->files = std::vector<std::string>();
    this->fo = new fileoperator(this->dir); // File and directory browser
    std::cout << "Connection to client '" << this->hostAddress << "' established" << std::endl;
}

//! Функция проверки пользовательского ввода на совпадение с существующей командой
bool serverconnection::commandEquals(std::string a, std::string b) {
    std::transform(a.begin(), a.end(),a.begin(), tolower);
    int found = a.find(b);
    return (found!=std::string::npos);
}

//! Парсер комнад
// Command switch for the issued client command, only called when this->command is set to 0
std::string serverconnection::commandParser(std::string command) {
    this->parameter;
    std::string res = "";
    this->uploadCommand = false;
    struct stat Status;
    // Commands can have either 0 or 1 parameters, e.g. 'browse' or 'browse ./'
    std::vector<std::string> commandAndParameter = this->extractParameters(command);

    //for (int i = 0; i < parameters.size(); i++) std::cout << "P " << i << ":" << parameters.at(i) << ":" << std::endl;
    std::cout << "Connection " << this->connectionId << ": ";

    // Количество аргументов у команыды равн 0
    if (commandAndParameter.size() == 1) {
        if (this->commandEquals(commandAndParameter.at(0), "list")) {

            // Директория для вывода
            std::string curDir = "./";
            std::cout << "Browsing files of the current working dir" << std::endl;
            this->directories.clear();
            this->files.clear();
            this->fo->browse("./", directories, files);
            std::cout << "Test" << std::endl;

            for (unsigned int j = 0; j < directories.size(); j++) {
                res += directories.at(j) + "/\n";
            }
            for (unsigned int i = 0; i < files.size(); i++) {
                res += files.at(i) + "\n";
            }
        } else
        if (this->commandEquals(commandAndParameter.at(0), "pwd")) { 
            std::cout << "Working dir requested" << std::endl;
            res = this->fo->getCurrentWorkingDir(false);
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getparentdir")) { 
            std::cout << "Parent dir of working dir requested" << std::endl;
            res = this->fo->getParentDir();
        } else
        if (this->commandEquals(commandAndParameter.at(0), "bye") || this->commandEquals(commandAndParameter.at(0), "quit")) {
            std::cout << "Shutdown of connection requested" << std::endl;
            this->closureRequested = true;
        } else {
     
            std::cout << "Unknown command encountered!" << std::endl;
            commandAndParameter.clear();
        }
    } else 
    if (commandAndParameter.size() > 1) {
        
        this->parameter = commandAndParameter.at(1);        
        if (this->commandEquals(commandAndParameter.at(0), "ls")) {
            std::string curDir = std::string(commandAndParameter.at(1));
            std::cout << "Browsing files of directory '" << curDir << "'" << std::endl;
            this->directories.clear();
            this->files.clear();
            this->fo->browse(curDir,directories,files);

            for (unsigned int j = 0; j < directories.size(); j++) {
                res += directories.at(j) + "/\n";
            }
            for (unsigned int i = 0; i < files.size(); i++) {
                res += files.at(i) + "\n";
            }
        } else
        if (this->commandEquals(commandAndParameter.at(0), "download")) {
            this->downloadCommand = true;
            std::cout << "Preparing download of file '" << this->parameter << "'" << std::endl;
            unsigned long lengthInBytes = 0;
            char* fileBlock;
            unsigned long readBytes = 0;
            std::stringstream st;
            if (!this->fo->readFile(this->parameter)) {
                    st.clear();
                    fileBlock = this->fo->readFileBlock(lengthInBytes);
                    st << lengthInBytes;
                    readBytes += lengthInBytes;
                    this->sendToClient(fileBlock,lengthInBytes); 
            }
            this->closureRequested = true;
        } else
        if (this->commandEquals(commandAndParameter.at(0), "upload")) {
            this->uploadCommand = true; 
            std::cout << "Preparing download of file '" << this->parameter << "'" << std::endl;
            res = this->fo->beginWriteFile(this->parameter);
        } else
        if (this->commandEquals(commandAndParameter.at(0), "cd")) { 
            std::cout << "Change of working dir to '" << this->parameter << "' requested" << std::endl;
            // Test if dir exists
            if (!this->fo->changeDir(this->parameter)) {
                std::cout << "Directory change to '" << this->parameter << "' successful!" << std::endl;
            }
            res = this->fo->getCurrentWorkingDir(false); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "rmdir")) {
            std::cout << "Deletion of dir '" << this->parameter << "' requested" << std::endl;
            if (this->fo->dirIsBelowServerRoot(this->parameter)) {
                std::cerr << "Attempt to delete directory beyond server root (prohibited)" << std::endl;
                res = "//"; 
            } else {
                this->directories.clear(); 
                this->fo->clearListOfDeletedDirectories();
                this->files.clear(); 
                this->fo->clearListOfDeletedFiles();
                if (this->fo->deleteDirectory(this->parameter)) {
                    std::cerr << "Error when trying to delete directory '" << this->parameter << "'" << std::endl;
                }
                this->directories = this->fo->getListOfDeletedDirectories();
                this->files = this->fo->getListOfDeletedFiles();
                for (unsigned int j = 0; j < directories.size(); j++) {
                    res += directories.at(j) + "\n";
                }
                for (unsigned int i = 0; i < files.size(); i++) {
                    res += files.at(i) + "\n";
                }
            }
        } else
        if (this->commandEquals(commandAndParameter.at(0), "delete")) {
            std::cout << "Deletion of file '" << this->parameter << "' requested" << std::endl;
            this->fo->clearListOfDeletedFiles();
            if (this->fo->deleteFile(this->parameter)) {
                res = "//";
            } else {
                std::vector<std::string> deletedFile = this->fo->getListOfDeletedFiles();
                if (deletedFile.size() > 0)
                    res = deletedFile.at(0);
            }
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getsize")) {
            std::cout << "Size of file '" << this->parameter << "' requested" << std::endl;
            std::vector<std::string> fileStats = this->fo->getStats(this->parameter, Status);
            res = fileStats.at(4);
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getaccessright")) {
            std::cout << "Access rights of file '" << this->parameter << "' requested" << std::endl;
            std::vector<std::string> fileStats = this->fo->getStats(this->parameter, Status);
            res = fileStats.at(0); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getlastmodificationtime")) {
            std::cout << "Last modification time of file '" << this->parameter << "' requested" << std::endl;
            std::vector<std::string> fileStats = this->fo->getStats(this->parameter, Status);
            res = fileStats.at(3); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getowner")) {
            std::cout << "Owner of file '" << this->parameter << "' requested" << std::endl;
            std::vector<std::string> fileStats = this->fo->getStats(this->parameter, Status);
            res = fileStats.at(2); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "getgroup")) {
            std::cout << "Group of file '" << this->parameter << "' requested" << std::endl;
            std::vector<std::string> fileStats = this->fo->getStats(this->parameter, Status);
            res = fileStats.at(1); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "mkdir")) { 
            std::cout << "Creating of dir '" << this->parameter << "' requested" << std::endl;
            res = (this->fo->createDirectory(this->parameter) ? "//" : this->parameter); 
        } else
        if (this->commandEquals(commandAndParameter.at(0), "touch")) { 
            std::cout << "Creating of empty file '" << this->parameter << "' requested" << std::endl;
            res = (this->fo->createFile(this->parameter) ? "//" : this->parameter);  
        } else {
            std::cout << "Unknown command encountered!" << std::endl;
            commandAndParameter.clear();
            command = "";
            res = "ERROR: Unknown command!";
        }
    } else 
    if (!commandAndParameter.at(0).empty()) {
        std::cout << "Unknown command encountered!" << std::endl;
        std::cout << std::endl;
        commandAndParameter.clear();
    }
    res += "\n";
    return res;
}


// Разбивает данные на  команду и параметры от клиента
std::vector<std::string> serverconnection::extractParameters(std::string command) {
    std::vector<std::string> res = std::vector<std::string>();
    std::size_t previouspos = 0;
    std::size_t pos;


    //! Берет команду и пробегается по все строке до первого пробела
    if ((pos = command.find(SEPARATOR, previouspos)) != std::string::npos) {
        res.push_back(command.substr(int(previouspos),int(pos-previouspos))); 
    }
    if (command.length() > (pos+1)) {
        res.push_back(command.substr(int(pos+1),int(command.length()-(pos+(this->commandOffset))))); 
    }
    return res;
}


//! Получает ывходящие данные и выдает соответствующие команды и ответы
void serverconnection::respondToQuery() {
    char buffer[BUFFER_SIZE];
    int bytes;
    bytes = recv(this->fd, buffer, sizeof(buffer), 0);

    // В не-блокирующем режимк, байты меньше 0 означают закрытие соединения
    if (bytes > 0) {
        std::string clientCommand = std::string(buffer, bytes);
        if (this->uploadCommand) { 
            std::cout << "Part " << ++(this->receivedPart) << ": ";
            this->fo->writeFileBlock(clientCommand);
        } else {
            std::string res = this->commandParser(clientCommand);
                this->sendToClient(res); 
        }
    } else { 
        if (this->uploadCommand) { 
            this->uploadCommand = false;
            this->downloadCommand = false;
            this->closureRequested = true;
            this->receivedPart = 0;
        }
    }
}


//! Отправляет заданную строку к клиенту, используя текущее соединение
void serverconnection::sendToClient(char* response, unsigned long length) {
    unsigned int bytesSend = 0;
    while (bytesSend < length) {
        int ret = send(this->fd, response+bytesSend, length-bytesSend, 0);
        if (ret <= 0) {
            return;
        }
        bytesSend += ret;
    }
}


//! Отправляет заданную строку к клиенту, используя текущее соединение
void serverconnection::sendToClient(std::string response) {
    unsigned int bytesSend = 0;
    while (bytesSend < response.length()) {
        int ret = send(this->fd, response.c_str()+bytesSend, response.length()-bytesSend, 0);
        if (ret <= 0) {
            return;
        }
        bytesSend += ret;
    }
}


//! Возвращает файловый дескриптор установленного текущего соединения
int serverconnection::getFD() {
    return this->fd;
}

//! Возвращает, в случае запрос на закрытия соединения было отправлено с клиента
bool serverconnection::getCloseRequestStatus() {
    return this->closureRequested;
}


unsigned int serverconnection::getConnectionId() {
    return this->connectionId;
}
