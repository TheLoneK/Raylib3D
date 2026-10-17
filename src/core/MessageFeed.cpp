#include "core/MessageFeed.h"
#include <algorithm>

void MessageFeed::Push(const std::string &text, Color color, float duration)
{
    auto existing = std::find_if(m_messages.begin(), m_messages.end(), [&](const Message &message) { return message.text == text; });
    if (existing != m_messages.end())
    {
        existing->timeLeft = std::max(existing->timeLeft, duration);
        return;
    }

    m_messages.push_back(Message{ text, color, duration });
    while (static_cast<int>(m_messages.size()) > MaxMessages) m_messages.pop_front();
}

void MessageFeed::Update(float deltaTime)
{
    for (Message &message : m_messages) message.timeLeft -= deltaTime;
    m_messages.erase(std::remove_if(m_messages.begin(), m_messages.end(), [](const Message &message) { return message.timeLeft <= 0.0f; }), m_messages.end());
}
