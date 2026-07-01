#include <iostream>
#include <string>
#include <vector>

struct Message
{
    std::string speaker;
    std::string text;
};

class ChatSession
{
public:
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

    void printWelcome() const
    {
        std::cout << "로컬 콘솔 채팅 프로그램 1단계\n";
        std::cout << "명령어가 궁금하면 /help 를 입력하세요.\n";
    }

    void handleInput(const std::string& userInput)
    {
        if (userInput.empty())
        {
            std::cout << "빈 문장은 대화 기록에 저장하지 않습니다.\n";
            return;
        }

        if (userInput == "/help" || userInput == "/history")
        {
            handleCommand(userInput);
            return;
        }

        addMessage("User", userInput);
        respondToNormalMessage();
    }

    void handleCommand(const std::string& command)
    {
        if (command == "/help")
        {
            addMessage("User", command);
            printHelp();
            addMessage("Bot", "명령어 목록을 출력했습니다.");
            return;
        }

        if (command == "/history")
        {
            printHistory();
            addMessage("User", command);
            return;
        }
    }

    void respondToNormalMessage()
    {
        std::string botResponse = createBotResponse();
        std::cout << "Bot: " << botResponse << "\n";

        addMessage("Bot", botResponse);
    }

    void printHelp() const
    {
        std::cout << "\n사용 가능한 명령어\n";
        std::cout << "  /help    : 명령어 목록을 보여줍니다.\n";
        std::cout << "  /history : 지금까지의 대화 기록을 보여줍니다.\n";
        std::cout << "  /exit    : 프로그램을 종료합니다.\n";
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

    std::string createBotResponse() const
    {
        return "아직은 간단한 봇입니다. 다음 단계에서 더 똑똑해질 예정입니다.";
    }

    void addMessage(const std::string& speaker, const std::string& text)
    {
        Message message;
        message.speaker = speaker;
        message.text = text;

        history.push_back(message);
    }
};

int main()
{
    ChatSession chatSession;
    chatSession.run();

    return 0;
}
