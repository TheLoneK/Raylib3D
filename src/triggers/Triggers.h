#pragma once

#include "flecs.h"
#include "triggers/TriggerChannels.h"

//----------------------------------------------------------------------------------
// Trigger channels connect a sender (a wall switch) to any number of receivers.
// A receiver is any entity with a TriggerTarget on the channel; Send emits a Triggered
// event to each one, and the receiver's own feature reacts with an observer, e.g.
//   DoorOpener   (items/Interactions)   opens the doors with its link tag
//   PathFollower (platforms/Platforms)  starts / stops the platform
// Senders never know what they are connected to.
//----------------------------------------------------------------------------------
namespace Triggers
{
    // Emits Triggered to every receiver on the channel. Returns how many received it
    int Send(flecs::world &world, int channel, flecs::entity source, bool on = true);

    // Makes an entity a receiver on the channel
    void Listen(flecs::entity entity, int channel);
}
