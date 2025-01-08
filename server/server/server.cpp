#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <locale>
#include <codecvt>
#include <map>
#include <set>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
using namespace std;

// Преобразование строки в UTF-8
string to_utf8(const wstring& wstr) {
    wstring_convert<codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr);
}

// Преобразование строки из UTF-8
wstring from_utf8(const string& str) {
    wstring_convert<codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
}

// Карта для хранения подключенных клиентов и их юзернеймов
map<SOCKET, wstring> clients;

// Структура для хранения данных игры "Быки и коровы"
struct Game {
    SOCKET host;               // Сокет ведущего (загадчика)
    wstring secretNumber;      // Загаданное число
    bool inProgress = false;   // Игра активна или нет
    set<SOCKET> participants;  // Участники игры
};

Game game;  // Единственная активная игра

// Широковещательная функция для отправки сообщений всем клиентам
void broadcastMessage(const wstring& message, SOCKET senderSocket = INVALID_SOCKET) {
    string utf8Message = to_utf8(message);
    for (const auto& client : clients) {
        if (client.first != senderSocket) {
            send(client.first, utf8Message.c_str(), utf8Message.size() + 1, 0);
        }
    }
}

// Функция для отправки сообщения определённому клиенту
void sendMessageToClient(SOCKET clientSocket, const wstring& message) {
    string utf8Message = to_utf8(message);
    send(clientSocket, utf8Message.c_str(), utf8Message.size() + 1, 0);
}

// Логика проверки "Быков и Коров"
pair<int, int> checkBullsAndCows(const wstring& guess, const wstring& secret) {
    int bulls = 0, cows = 0;
    for (int i = 0; i < 5; i++) {
        if (guess[i] == secret[i]) {
            bulls++;
        }
        else if (find(secret.begin(), secret.end(), guess[i]) != secret.end()) {
            cows++;
        }
    }
    return { bulls, cows };
}

// Функция для получения списка пользователей
wstring getUserList() {
    wstring userList = L"Пользователи онлайн:\n";
    for (const auto& client : clients) {
        userList += L"- " + client.second + L"\n";
    }
    return userList;
}

// Обработка каждого клиента
void handleClient(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;

    // Получаем юзернейм
    bytesReceived = recv(clientSocket, buffer, 1024, 0);
    if (bytesReceived > 0) {
        string usernameUtf8(buffer, 0, bytesReceived);
        wstring username = from_utf8(usernameUtf8);
        clients[clientSocket] = username;
        wcout << L"Пользователь " << username << L" подключился.\n";

        // Отправляем приветствие клиенту
        wstring welcomeMessage = L"Привет, " + username + L"! Вы подключены к серверу.";
        sendMessageToClient(clientSocket, welcomeMessage);

        // Информируем других клиентов
        broadcastMessage(L"Пользователь " + username + L" подключился к чату.", clientSocket);
    }

    while (true) {
        ZeroMemory(buffer, 1024);
        bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived > 0) {
            string receivedStr(buffer, 0, bytesReceived);
            wstring message = from_utf8(receivedStr);

            // Команда JOIN
            if (message == L"JOIN") {
                if (game.inProgress) {
                    if (game.participants.find(clientSocket) == game.participants.end()) {
                        game.participants.insert(clientSocket);
                        sendMessageToClient(clientSocket, L"Вы присоединились к игре 'Быки и Коровы'.");
                        broadcastMessage(L"Игрок " + clients[clientSocket] + L" присоединился к игре.");
                    }
                    else {
                        sendMessageToClient(clientSocket, L"Вы уже участвуете в игре.");
                    }
                }
                else {
                    sendMessageToClient(clientSocket, L"Игра еще не началась.");
                }
            }

            // Если игра активна
            else if (game.inProgress) {
                if (game.participants.find(clientSocket) != game.participants.end()) {
                    if (message.size() == 5 && all_of(message.begin(), message.end(), ::iswdigit)) {
                        // Проверка догадки
                        pair<int, int> result = checkBullsAndCows(message, game.secretNumber);
                        wstring resultMessage = L"Быки: " + to_wstring(result.first) + L", Коровы: " + to_wstring(result.second);
                        sendMessageToClient(clientSocket, resultMessage);
                        if (result.first == 5) {
                            broadcastMessage(L"Игрок " + clients[clientSocket] + L" угадал число! Игра окончена.");
                            game.inProgress = false;
                            game.participants.clear(); // Очистить участников
                        }
                    }
                    else {
                        sendMessageToClient(clientSocket, L"Введите пятизначное число.");
                    }
                }
                else {
                    sendMessageToClient(clientSocket, L"Вы не участвуете в этой игре.");
                }
            }

            // Команда START
            else if (message.substr(0, 6) == L"START ") {
                if (!game.inProgress) {
                    game.secretNumber = message.substr(6, 5);
                    game.host = clientSocket;
                    game.inProgress = true;
                    game.participants.clear(); // Очистить участников
                    game.participants.insert(clientSocket); // Добавить ведущего
                    broadcastMessage(L"Игра 'Быки и Коровы' началась! Попробуйте угадать число.");
                }
                else {
                    sendMessageToClient(clientSocket, L"Игра уже идет!");
                }
            }

            // Команда HELLO
            else if (message == L"HELLO") {
                wstring response = L"Привет, " + clients[clientSocket] + L"!";
                sendMessageToClient(clientSocket, response);
                broadcastMessage(L"Пользователь " + clients[clientSocket] + L" говорит HELLO.", clientSocket);
            }
            // Команда BYE
            else if (message == L"BYE") {
                wstring goodbyeMessage = L"Пользователь " + clients[clientSocket] + L" вышел из чата.";
                broadcastMessage(goodbyeMessage, clientSocket);
                sendMessageToClient(clientSocket, L"До свидания, " + clients[clientSocket] + L"!");
                closesocket(clientSocket);
                clients.erase(clientSocket);
                wcout << L"Пользователь " << clients[clientSocket] << L" отключился.\n";
                break;
            }
            // Команда HELP
            else if (message == L"HELP") {
                wstring helpMessage = L"Доступные команды:\nHELLO - приветствие\nBYE - выход из чата\nHELP - список команд\nUSERS - список пользователей онлайн\nSTART XXXXX - начать игру\nJOIN - присоедится к игре";
                sendMessageToClient(clientSocket, helpMessage);
            }
            // Команда USERS
            else if (message == L"USERS") {
                wstring userList = getUserList();
                sendMessageToClient(clientSocket, userList);
            }
            // Обычное сообщение
            else {
                wstring broadcast = clients[clientSocket] + L": " + message;
                broadcastMessage(broadcast, clientSocket);
            }
        }
        else {
            break;
        }
    }
}

string getLocalIPAddress() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct addrinfo hints;
    struct addrinfo* result;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo(hostname, NULL, &hints, &result);
    if (res != 0) {
        cerr << "getaddrinfo failed: " << gai_strerror(res) << endl;
        return "";
    }

    char ipAddress[INET_ADDRSTRLEN]; // Для IPv4
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET) {
            struct sockaddr_in* sockaddr_ipv4 = reinterpret_cast<struct sockaddr_in*>(ptr->ai_addr);
            inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));
            break;
        }
    }

    freeaddrinfo(result);
    return ipAddress;
}

int main() {
    setlocale(LC_ALL, "Russian");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Запрашиваем у пользователя порт
    int port;
    cout << "Введите порт сервера: ";
    cin >> port;

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Привязываем сокет к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Ошибка привязки сокета: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Получаем и выводим локальный IP-адрес и порт
    string localIP = getLocalIPAddress();
    cout << "Сервер запущен на " << localIP << ":" << port << endl;

    listen(serverSocket, SOMAXCONN);

    // Основной цикл для ожидания подключения клиентов
    cout << "Ожидание подключения клиентов..." << endl;

    vector<thread> clientThreads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        cout << "Клиент подключился!" << endl;
        clientThreads.push_back(thread(handleClient, clientSocket));
    }

    for (thread& t : clientThreads) {
        t.join();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}