#include <vast/store/archive.h>

#include <vast/store/emitter.h>
#include <vast/store/exception.h>
#include <vast/store/segmentizer.h>
#include <vast/store/segment_manager.h>
#include <vast/util/logger.h>

namespace vast {
namespace store {

archive::archive(std::string const& directory,
                 size_t max_events_per_chunk,
                 size_t max_segment_size,
                 size_t max_segments)
{
  using namespace cppa;
  segment_manager_ = spawn<segment_manager>(max_segments, directory);
  segmentizer_ = spawn<segmentizer>(segment_manager_,
                                    max_events_per_chunk,
                                    max_segment_size);

  init_state = (
      on(atom("emitter"), atom("create"), arg_match) >> [=](actor_ptr sink)
      {
        auto em = spawn<emitter>(segment_manager_);
        emitters_.push_back(em);
        send(em, atom("set"), atom("sink"), sink);
        reply(atom("emitter"), atom("create"), atom("ack"), em);
      },
      on_arg_match >> [=](ze::event const& e)
      {
        segmentizer_ << self->last_dequeued();
      },
      on(atom("shutdown")) >> [=]()
      {
        segmentizer_ << self->last_dequeued();
        become(
            keep_behavior,
            on(atom("shutdown"), atom("ack")) >> [=]
            {
              LOG(debug, store) << "received segmentizer shutdown ack";

              send(segment_manager_, atom("shutdown"));
              for (auto em : emitters_)
                send(em, atom("shutdown"));

              self->quit();
              LOG(verbose, store) << "archive terminated";
            });
      });
}

} // namespace store
} // namespace vast
