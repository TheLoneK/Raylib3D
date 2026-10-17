#pragma once

#include "raylib.h"
#include <deque>
#include <string>

//----------------------------------------------------------------------------------
// Short on-screen notices ("Picked up the RED key", "+24 Rifle ammo"...).
// Pushing the same text again refreshes it instead of stacking duplicates.
//----------------------------------------------------------------------------------
class MessageFeed
{
public:
    struct Message
    {
        std::string text;
        Color color;
        float timeLeft;
    };

    static constexpr int MaxMessages = 4;
    static constexpr float DefaultDuration = 3.0f;

    void Push(const std::string &text, Color color = WHITE, float duration = DefaultDuration);
    void Update(float deltaTime);
    void Clear() { m_messages.clear(); }
    const std::deque<Message> &GetMessages() const { return m_messages; }

private:
    std::deque<Message> m_messages;     // Oldest first
};
