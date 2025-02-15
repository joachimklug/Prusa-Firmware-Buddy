#pragma once

#include "changes.hpp"
#include "command.hpp"
#include "sleep.hpp"

#include <common/shared_buffer.hpp>
#include <transfers/monitor.hpp>
#include <transfers/download.hpp>
#include <transfers/changed_path.hpp>

#include <cstdint>
#include <optional>
#include <variant>

namespace connect_client {

class Printer;

struct SendTelemetry {
    bool empty;
};

enum class EventType {
    Info,
    JobInfo,
    FileInfo,
    FileChanged,
    TransferInfo,
    Rejected,
    Accepted,
    Finished,
    Failed,
    TransferStopped,
    TransferAborted,
    TransferFinished,
};

const char *to_str(EventType event);

struct Event {
    EventType type;
    std::optional<CommandId> command_id { std::nullopt };
    std::optional<uint16_t> job_id { std::nullopt };
    std::optional<SharedPath> path { std::nullopt };
    std::optional<transfers::TransferId> transfer_id { std::nullopt };
    /// Reason for the event. May be null.
    ///
    /// Reasons are constant strings, therefore the non-owned const char * ‒
    /// they are not supposed to get "constructed" or interpolated.
    const char *reason = nullptr;
    bool is_file = false;
    transfers::ChangedPath::Incident incident {};
    std::optional<CommandId> start_cmd_id { std::nullopt };
};

using Action = std::variant<
    SendTelemetry,
    Event,
    Sleep>;

enum class ActionResult {
    Ok,
    Failed,
    Refused,
};

/// The planner
///
/// This part is responsible for coordinating what happens when. As we have
/// only one connection to the Connect servers, we can issue only one request at
/// a time. We need to decide which one to do right now (if any) and react to
/// commands from the server.
///
/// It is also responsible for doing retries, re-initializing communication and
/// similar after something bad happens.
class Planner {
private:
    struct BackgroundCommand {
        CommandId id;
        BackgroundCmd command;
    };

    Printer &printer;

    /// The next (or current) event we want to send out.
    std::optional<Event> planned_event;
    /// Last time we've successfully sent a telemetry to the server.
    std::optional<Timestamp> last_telemetry;
    /// Last time we've successfully talked to the server.
    std::optional<Timestamp> last_success;
    /// When doing comm retries, this is the cooldown time between them.
    ///
    /// In case we are in a unsuccessfull row, this is keeping the current
    /// value, which gets incremented each time (up to a limit).
    std::optional<Duration> cooldown;
    /// The next action shall be a cooldown.
    ///
    /// Note that this might be set to false and still have the above cooldown
    /// set. In that case we just did the cooldown, can retry right now, but we
    /// want to keep the value in case the retry also fails.
    bool perform_cooldown;

    /// How many times we've tried and failed due to some kind of network error
    /// or such? After enough, we give up sending a specific event because the
    /// failure might be related to that specific event in some way (in theory,
    /// it should not, they should be detected as Refused instead of Failed,
    /// but there are always corner cases).
    uint8_t failed_attempts = 0;

    /// Some command that is accepted but still being worked on.
    std::optional<BackgroundCommand> background_command;

    // Handlers for specific commands.
    void command(const Command &, const BrokenCommand &);
    void command(const Command &, const UnknownCommand &);
    void command(const Command &, const GcodeTooLarge &);
    void command(const Command &, const ProcessingThisCommand &);
    void command(const Command &, const ProcessingOtherCommand &);
    void command(const Command &, const Gcode &);
    void command(const Command &, const SendInfo &);
    void command(const Command &, const SendJobInfo &);
    void command(const Command &, const SendFileInfo &);
    void command(const Command &, const SendTransferInfo &);
    void command(const Command &, const PausePrint &);
    void command(const Command &, const ResumePrint &);
    void command(const Command &, const StopPrint &);
    void command(const Command &, const StartPrint &);
    void command(const Command &, const CancelPrinterReady &);
    void command(const Command &, const SetPrinterReady &);
    void command(const Command &, const StartConnectDownload &);
    void command(const Command &, const DeleteFile &);
    void command(const Command &, const DeleteFolder &);
    void command(const Command &, const CreateFolder &);
    void command(const Command &, const StopTransfer &);

    // Tracking if we should resend the INFO message due to some changes.
    Tracked info_changes;
    // Tracking of ongoing transfers.
    std::optional<transfers::TransferId> observed_transfer;

    // We are able to resume _encrypted_ downloads.
    //
    // In theory, we could also try to recover plain-text ones. But they need a
    // lot more info for that to work, and we won't be using them in practice
    // (likely someone may be using them on a private copy of Connect, but that
    // will likely happen on a local network that should be reliable, not on
    // the wide broken internet).
    //
    // In case of a plain-text download, we simply set the number of allowed
    // retries to 0 to "disable" them.
    struct ResumableDownload {
        transfers::Download download;
        uint32_t orig_size = 0;
        std::optional<uint16_t> port { std::nullopt };
        transfers::Decryptor::Block orig_iv {};
        bool need_retry = false;
        uint8_t allowed_retries = 0;
    };
    // A download running in background.
    //
    // As we may have a background _task_ and a download at the same time, we
    // need to have variables for both.
    std::optional<ResumableDownload> download;

    std::optional<CommandId> transfer_start_cmd = std::nullopt;
    std::optional<CommandId> print_start_cmd = std::nullopt;

    /// Constructs corresponding Sleep action.
    Sleep sleep(Duration duration);

public:
    Planner(Printer &printer)
        : printer(printer) {
        reset();
    }
    /// Reset the state.
    ///
    /// This is for the "severe" cases, for example when connect is disabled
    /// and enabled or its configuration changes (and we may be possibly talking
    /// to another server).
    void reset();
    /// A command was received from the server.
    ///
    /// The caller is responsible only for handing it over. No automatic acks
    /// or rejects or handling on the caller side is expected.
    void command(Command command);
    /// What should the caller do next.
    ///
    /// Returns the next action to take. It may be a sleep (in which case the
    /// caller may take a nap but in case something happens ‒ eg. a command
    /// arrives, it might interact sooner).
    ///
    /// All actions except sleeps expect a follow-up call to action_done.
    Action next_action(SharedBuffer &buffer);
    // Note: *Not* for Sleep. Only for stuff that sends.
    void action_done(ActionResult action);

    // Only for Success/Failure.
    void background_done(BackgroundResult result);
    void download_done(transfers::DownloadStep result);
    void recover_download();

    // ID of a command being executed in the background, if any.
    std::optional<CommandId> background_command_id() const;
};

}
