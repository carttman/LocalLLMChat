#include <iostream>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif

struct Message
{
    std::string speaker;
    std::string text;
};

class OllamaClient
{
public:
    std::string createChatResponse(const std::vector<Message>& history,
        const std::string& modelName,
        std::string& errorMessage) const
    {
        std::string requestBody = buildChatRequestBody(history, modelName);
        std::string responseBody = sendPostRequest(L"/api/chat", requestBody, errorMessage);

        if (!errorMessage.empty())
        {
            return "";
        }

        std::size_t messagePosition = responseBody.find("\"message\"");
        std::size_t errorPosition = responseBody.find("\"error\"");

        std::string ollamaError;
        if (errorPosition != std::string::npos &&
            (messagePosition == std::string::npos || errorPosition < messagePosition) &&
            extractJsonStringValue(responseBody, "error", errorPosition, ollamaError))
        {
            errorMessage = "Ollama 오류: " + ollamaError;
            return "";
        }

        std::string content;

        if (messagePosition == std::string::npos ||
            !extractJsonStringValue(responseBody, "content", messagePosition, content))
        {
            errorMessage = "Ollama 응답에서 답변 내용을 찾지 못했습니다.";
            return "";
        }

        return content;
    }

private:
    std::string buildChatRequestBody(const std::vector<Message>& history,
        const std::string& modelName) const
    {
        std::string body;

        body += "{";
        body += "\"model\":\"" + escapeJson(modelName) + "\",";
        body += "\"stream\":false,";
        body += "\"messages\":[";

        appendJsonMessage(body,
            "system",
            "너는 로컬 C++ 콘솔 채팅 프로그램에 연결된 AI입니다. 한국어로 친절하고 간단하게 답하세요.");

        for (int i = 0; i < static_cast<int>(history.size()); ++i)
        {
            if (!history[i].text.empty() && history[i].text[0] == '/')
            {
                continue;
            }

            body += ",";

            if (history[i].speaker == "User")
            {
                appendJsonMessage(body, "user", history[i].text);
            }
            else
            {
                appendJsonMessage(body, "assistant", history[i].text);
            }
        }

        body += "]";
        body += "}";

        return body;
    }

    void appendJsonMessage(std::string& body,
        const std::string& role,
        const std::string& content) const
    {
        body += "{";
        body += "\"role\":\"" + escapeJson(role) + "\",";
        body += "\"content\":\"" + escapeJson(content) + "\"";
        body += "}";
    }

    std::string sendPostRequest(const std::wstring& path,
        const std::string& requestBody,
        std::string& errorMessage) const
    {
        HINTERNET session = WinHttpOpen(L"LocalLLMChat/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (session == NULL)
        {
            errorMessage = makeWindowsErrorMessage("WinHttpOpen");
            return "";
        }

        WinHttpSetTimeouts(session, 5000, 5000, 10000, 180000);

        HINTERNET connection = WinHttpConnect(session, L"localhost", 11434, 0);

        if (connection == NULL)
        {
            errorMessage = makeWindowsErrorMessage("WinHttpConnect");
            WinHttpCloseHandle(session);
            return "";
        }

        HINTERNET request = WinHttpOpenRequest(connection,
            L"POST",
            path.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0);

        if (request == NULL)
        {
            errorMessage = makeWindowsErrorMessage("WinHttpOpenRequest");
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return "";
        }

        std::wstring headers = L"Content-Type: application/json\r\n";

        BOOL sendResult = WinHttpSendRequest(request,
            headers.c_str(),
            static_cast<DWORD>(headers.length()),
            const_cast<char*>(requestBody.c_str()),
            static_cast<DWORD>(requestBody.size()),
            static_cast<DWORD>(requestBody.size()),
            0);

        if (!sendResult)
        {
            errorMessage = makeWindowsErrorMessage("WinHttpSendRequest");
            closeHandles(request, connection, session);
            return "";
        }

        if (!WinHttpReceiveResponse(request, NULL))
        {
            errorMessage = makeWindowsErrorMessage("WinHttpReceiveResponse");
            closeHandles(request, connection, session);
            return "";
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);

        WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusCodeSize,
            WINHTTP_NO_HEADER_INDEX);

        std::string responseBody = readResponseBody(request, errorMessage);

        closeHandles(request, connection, session);

        if (!errorMessage.empty())
        {
            return "";
        }

        if (statusCode < 200 || statusCode >= 300)
        {
            errorMessage = "Ollama HTTP 상태 코드: " + std::to_string(statusCode);
            return responseBody;
        }

        return responseBody;
    }

    std::string readResponseBody(HINTERNET request, std::string& errorMessage) const
    {
        std::string responseBody;
        DWORD bytesAvailable = 0;

        while (true)
        {
            if (!WinHttpQueryDataAvailable(request, &bytesAvailable))
            {
                errorMessage = makeWindowsErrorMessage("WinHttpQueryDataAvailable");
                return "";
            }

            if (bytesAvailable == 0)
            {
                break;
            }

            std::vector<char> buffer(bytesAvailable);
            DWORD bytesRead = 0;

            if (!WinHttpReadData(request, buffer.data(), bytesAvailable, &bytesRead))
            {
                errorMessage = makeWindowsErrorMessage("WinHttpReadData");
                return "";
            }

            responseBody.append(buffer.data(), bytesRead);
        }

        return responseBody;
    }

    void closeHandles(HINTERNET request, HINTERNET connection, HINTERNET session) const
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
    }

    std::string makeWindowsErrorMessage(const std::string& functionName) const
    {
        return functionName + " 실패. Windows 오류 코드: " + std::to_string(GetLastError());
    }

    std::string escapeJson(const std::string& text) const
    {
        std::string escaped;
        const char hexDigits[] = "0123456789abcdef";

        for (int i = 0; i < static_cast<int>(text.size()); ++i)
        {
            unsigned char current = static_cast<unsigned char>(text[i]);

            if (current == '\"')
            {
                escaped += "\\\"";
            }
            else if (current == '\\')
            {
                escaped += "\\\\";
            }
            else if (current == '\n')
            {
                escaped += "\\n";
            }
            else if (current == '\r')
            {
                escaped += "\\r";
            }
            else if (current == '\t')
            {
                escaped += "\\t";
            }
            else if (current < 0x20)
            {
                escaped += "\\u00";
                escaped += hexDigits[current >> 4];
                escaped += hexDigits[current & 0x0F];
            }
            else
            {
                escaped += text[i];
            }
        }

        return escaped;
    }

    bool extractJsonStringValue(const std::string& json,
        const std::string& key,
        std::size_t searchStart,
        std::string& value) const
    {
        std::string keyText = "\"" + key + "\"";
        std::size_t keyPosition = json.find(keyText, searchStart);

        if (keyPosition == std::string::npos)
        {
            return false;
        }

        std::size_t colonPosition = json.find(':', keyPosition + keyText.size());

        if (colonPosition == std::string::npos)
        {
            return false;
        }

        std::size_t quotePosition = json.find('"', colonPosition + 1);

        if (quotePosition == std::string::npos)
        {
            return false;
        }

        return readJsonString(json, quotePosition, value);
    }

    bool readJsonString(const std::string& json,
        std::size_t quotePosition,
        std::string& value) const
    {
        value.clear();

        if (quotePosition >= json.size() || json[quotePosition] != '"')
        {
            return false;
        }

        std::size_t i = quotePosition + 1;

        while (i < json.size())
        {
            char current = json[i];

            if (current == '"')
            {
                return true;
            }

            if (current != '\\')
            {
                value += current;
                ++i;
                continue;
            }

            if (i + 1 >= json.size())
            {
                return false;
            }

            char escaped = json[i + 1];

            if (escaped == '"')
            {
                value += '"';
                i += 2;
            }
            else if (escaped == '\\')
            {
                value += '\\';
                i += 2;
            }
            else if (escaped == '/')
            {
                value += '/';
                i += 2;
            }
            else if (escaped == 'b')
            {
                value += '\b';
                i += 2;
            }
            else if (escaped == 'f')
            {
                value += '\f';
                i += 2;
            }
            else if (escaped == 'n')
            {
                value += '\n';
                i += 2;
            }
            else if (escaped == 'r')
            {
                value += '\r';
                i += 2;
            }
            else if (escaped == 't')
            {
                value += '\t';
                i += 2;
            }
            else if (escaped == 'u')
            {
                if (!readUnicodeEscape(json, i, value))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        return false;
    }

    bool readUnicodeEscape(const std::string& json,
        std::size_t& position,
        std::string& value) const
    {
        if (position + 5 >= json.size())
        {
            return false;
        }

        unsigned int codeUnit = 0;

        if (!readHex4(json, position + 2, codeUnit))
        {
            return false;
        }

        unsigned int codePoint = codeUnit;

        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF)
        {
            if (position + 11 >= json.size() ||
                json[position + 6] != '\\' ||
                json[position + 7] != 'u')
            {
                return false;
            }

            unsigned int lowCodeUnit = 0;

            if (!readHex4(json, position + 8, lowCodeUnit) ||
                lowCodeUnit < 0xDC00 ||
                lowCodeUnit > 0xDFFF)
            {
                return false;
            }

            codePoint = 0x10000 + ((codeUnit - 0xD800) << 10) + (lowCodeUnit - 0xDC00);
            position += 12;
        }
        else
        {
            position += 6;
        }

        appendUtf8(value, codePoint);
        return true;
    }

    bool readHex4(const std::string& text, std::size_t start, unsigned int& value) const
    {
        value = 0;

        if (start + 3 >= text.size())
        {
            return false;
        }

        for (int i = 0; i < 4; ++i)
        {
            int digit = hexValue(text[start + i]);

            if (digit < 0)
            {
                return false;
            }

            value = value * 16 + static_cast<unsigned int>(digit);
        }

        return true;
    }

    int hexValue(char value) const
    {
        if (value >= '0' && value <= '9')
        {
            return value - '0';
        }

        if (value >= 'a' && value <= 'f')
        {
            return value - 'a' + 10;
        }

        if (value >= 'A' && value <= 'F')
        {
            return value - 'A' + 10;
        }

        return -1;
    }

    void appendUtf8(std::string& text, unsigned int codePoint) const
    {
        if (codePoint <= 0x7F)
        {
            text += static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF)
        {
            text += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint <= 0xFFFF)
        {
            text += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
            text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else
        {
            text += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
            text += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }
};

class ChatSession
{
public:
    ChatSession()
        : modelName("llama3.2")
    {
    }

    void run()
    {
        printWelcome();

        std::string userInput;

        while (true)
        {
            std::cout << "\nYou: ";
            std::getline(std::cin, userInput);

            if (!std::cin)
            {
                std::cout << "\n입력이 종료되었습니다.\n";
                break;
            }

            if (userInput == "/exit")
            {
                addMessage("User", userInput);
                std::cout << "채팅을 종료합니다.\n";
                break;
            }

            handleInput(userInput);
        }
    }

private:
    std::vector<Message> history;
    OllamaClient ollamaClient;
    std::string modelName;

    void printWelcome() const
    {
        std::cout << "로컬 콘솔 채팅 프로그램 - Ollama 연동 버전\n";
        std::cout << "현재 모델: " << modelName << "\n";
        std::cout << "명령어가 궁금하면 /help 를 입력하세요.\n";
    }

    void handleInput(const std::string& userInput)
    {
        if (userInput.empty())
        {
            std::cout << "빈 문장은 대화 기록에 저장하지 않습니다.\n";
            return;
        }

        if (isCommand(userInput))
        {
            handleCommand(userInput);
            return;
        }

        addMessage("User", userInput);
        respondWithOllama();
    }

    bool isCommand(const std::string& userInput) const
    {
        return userInput == "/help" ||
            userInput == "/history" ||
            userInput == "/model" ||
            startsWith(userInput, "/model ");
    }

    void handleCommand(const std::string& command)
    {
        if (command == "/help")
        {
            addMessage("User", command);
            printHelp();
            return;
        }

        if (command == "/history")
        {
            printHistory();
            addMessage("User", command);
            return;
        }

        if (command == "/model")
        {
            addMessage("User", command);
            std::cout << "현재 모델: " << modelName << "\n";
            return;
        }

        if (startsWith(command, "/model "))
        {
            addMessage("User", command);
            changeModel(command.substr(7));
            return;
        }
    }

    void respondWithOllama()
    {
        std::cout << "Bot: 답변을 생성하는 중입니다...\n";

        std::string errorMessage;
        std::string botResponse = ollamaClient.createChatResponse(history, modelName, errorMessage);

        if (!errorMessage.empty())
        {
            botResponse = "Ollama와 통신하지 못했습니다. " + errorMessage;
            botResponse += "\nOllama가 실행 중인지, 모델이 설치되어 있는지 확인하세요.";
        }

        std::cout << "Bot: " << botResponse << "\n";
        addMessage("Bot", botResponse);
    }

    void changeModel(const std::string& newModelName)
    {
        if (newModelName.empty())
        {
            std::cout << "모델 이름이 비어 있습니다. 예: /model llama3.2\n";
            return;
        }

        modelName = newModelName;
        std::cout << "모델을 변경했습니다: " << modelName << "\n";
    }

    void printHelp() const
    {
        std::cout << "\n사용 가능한 명령어\n";
        std::cout << "  /help          : 명령어 목록을 보여줍니다.\n";
        std::cout << "  /history       : 지금까지의 대화 기록을 보여줍니다.\n";
        std::cout << "  /model         : 현재 Ollama 모델 이름을 보여줍니다.\n";
        std::cout << "  /model <이름>  : 사용할 Ollama 모델을 변경합니다.\n";
        std::cout << "  /exit          : 프로그램을 종료합니다.\n";
    }

    void printHistory() const
    {
        std::cout << "\n대화 기록\n";

        if (history.empty())
        {
            std::cout << "  아직 대화 기록이 없습니다.\n";
            return;
        }

        for (int i = 0; i < static_cast<int>(history.size()); ++i)
        {
            std::cout << i + 1 << ". ";
            std::cout << history[i].speaker << ": ";
            std::cout << history[i].text << "\n";
        }
    }

    void addMessage(const std::string& speaker, const std::string& text)
    {
        Message message;
        message.speaker = speaker;
        message.text = text;

        history.push_back(message);
    }

    bool startsWith(const std::string& text, const std::string& prefix) const
    {
        if (text.size() < prefix.size())
        {
            return false;
        }

        return text.substr(0, prefix.size()) == prefix;
    }
};

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    ChatSession chatSession;
    chatSession.run();

    return 0;
}
