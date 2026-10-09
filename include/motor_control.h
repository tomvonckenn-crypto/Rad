#pragma once
#include <stdint.h>
namespace rad {
enum class State : uint8_t { Locked, Ready, Running, Stopping, AwaitStandstill, Fault };
enum class Direction : uint8_t { None, Left, Right };
enum class Reason : uint8_t { Boot, None, Emergency, ConnectionLost, Stopped, Ota, OtaError };

// GPIO-independent logic. The actual methods are evaluated by the compiler
// in motor_checks.cpp, including heartbeat expiry and millis rollover.
class MotorControl {
 public:
  static constexpr uint16_t MaxPwm = 300;
  static constexpr uint32_t RampMs = 30;
  static constexpr uint32_t LeaseMs = 2000;
  constexpr State state() const { return state_; }
  constexpr Direction direction() const { return direction_; }
  constexpr Reason reason() const { return reason_; }
  constexpr uint16_t current() const { return current_; }
  constexpr uint16_t target() const { return target_; }
  constexpr bool leaseActive() const {
    return state_ == State::Ready || state_ == State::Running || state_ == State::Stopping;
  }
  // Caller must obtain a fresh explicit confirmation of physical standstill.
  constexpr bool arm(uint32_t now) {
    if (current_ != 0 || (state_ != State::Locked && state_ != State::AwaitStandstill)) return false;
    state_ = State::Ready; direction_ = Direction::None;
    target_ = current_ = 0; reason_ = Reason::None;
    lastHeartbeat_ = lastRamp_ = now;
    return true;
  }
  constexpr bool start(Direction direction, uint32_t now) {
    tick(now);
    if (state_ != State::Ready || direction == Direction::None) return false;
    direction_ = direction; state_ = State::Running;
    target_ = current_ = 0; // Never start at a saved speed.
    lastRamp_ = now;
    return true;
  }
  constexpr bool setTarget(uint16_t value) {
    if (state_ != State::Running || value > MaxPwm) return false;
    target_ = value; return true;
  }
  constexpr bool step(int delta) {
    if (state_ != State::Running || (delta != -1 && delta != 1)) return false;
    const int value = static_cast<int>(target_) + delta;
    target_ = value < 0 ? 0 : value > MaxPwm ? MaxPwm : static_cast<uint16_t>(value);
    return true;
  }
  constexpr bool heartbeat(uint32_t now) {
    tick(now); // Late heartbeats cannot revive an expired session.
    if (!leaseActive()) return false;
    lastHeartbeat_ = now; return true;
  }
  constexpr bool slowStop(uint32_t now) {
    tick(now);
    if (state_ == State::Stopping) return true;
    if (state_ != State::Running && state_ != State::Ready) return false;
    target_ = 0;
    state_ = current_ == 0 ? State::AwaitStandstill : State::Stopping;
    if (state_ == State::AwaitStandstill) reason_ = Reason::Stopped;
    return true;
  }
  constexpr void trip(Reason reason, bool permanent = false) {
    if (state_ == State::Fault && !permanent) return; // Stop must not clear an OTA fault.
    state_ = permanent ? State::Fault : State::Locked;
    reason_ = reason; target_ = current_ = 0; direction_ = Direction::None;
  }
  constexpr void tick(uint32_t now) {
    if (leaseActive() && static_cast<uint32_t>(now - lastHeartbeat_) >= LeaseMs) {
      trip(Reason::ConnectionLost); return;
    }
    if (state_ != State::Running && state_ != State::Stopping) return;
    if (static_cast<uint32_t>(now - lastRamp_) < RampMs) return;
    lastRamp_ = now; // No multi-step jump following a delayed loop.
    if (current_ < target_) ++current_;
    else if (current_ > target_) --current_;
    if (state_ == State::Stopping && current_ == 0) {
      state_ = State::AwaitStandstill; reason_ = Reason::Stopped;
    }
  }
 private:
  State state_ = State::Locked;
  Direction direction_ = Direction::None;
  Reason reason_ = Reason::Boot;
  uint16_t target_ = 0;
  uint16_t current_ = 0;
  uint32_t lastHeartbeat_ = 0;
  uint32_t lastRamp_ = 0;
};
}
