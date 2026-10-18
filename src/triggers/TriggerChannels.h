#pragma once

//----------------------------------------------------------------------------------
// Trigger channel numbering. Its own tiny header so the level generator can hand out
// channels without depending on the ECS.
//----------------------------------------------------------------------------------
namespace TriggerChannels
{
    // Channels below this are door link tags (LinkTags), so a door switch sends its door's link.
    // Everything else (platform switches...) gets a channel from here up
    inline constexpr int FirstFree = 100;
}
