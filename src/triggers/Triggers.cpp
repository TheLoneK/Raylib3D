#include "triggers/Triggers.h"
#include "ecs/Components.h"
#include <vector>

namespace Triggers
{
    int Send(flecs::world &world, int channel, flecs::entity source, bool on)
    {
        // Collect first: receivers may change components while handling the event
        std::vector<flecs::entity> receivers;
        world.each([&](flecs::entity entity, const TriggerTarget &target) {
            if (target.channel == channel) receivers.push_back(entity);
        });

        Triggered event = { channel, source.id(), on };
        for (flecs::entity receiver : receivers)
        {
            world.event<Triggered>().id<TriggerTarget>().entity(receiver).ctx(event).emit();
        }
        return static_cast<int>(receivers.size());
    }

    void Listen(flecs::entity entity, int channel)
    {
        entity.set(TriggerTarget{ channel });
    }
}
