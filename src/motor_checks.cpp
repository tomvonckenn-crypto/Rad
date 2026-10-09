#include "motor_control.h"
using namespace rad;
constexpr bool checkBootAndStart() {
  MotorControl m;
  if (m.current() || m.target() || m.state() != State::Locked) return false;
  if (m.start(Direction::Right, 0) || m.setTarget(1)) return false;
  if (!m.arm(0) || !m.start(Direction::Right, 0)) return false;
  return m.current() == 0 && m.target() == 0 && !m.start(Direction::Left, 0);
}
constexpr bool checkRampAndLimits() {
  MotorControl m; m.arm(0); m.start(Direction::Right, 0);
  if (m.setTarget(301) || m.target() != 0) return false;
  m.setTarget(300);
  m.tick(29); if (m.current() != 0) return false;
  m.tick(30); if (m.current() != 1) return false;
  m.tick(1000); if (m.current() != 2) return false;
  if (!m.step(1) || m.target() != 300 || m.step(2)) return false;
  m.setTarget(0); m.step(-1); return m.target() == 0;
}
constexpr bool checkGradualStop() {
  MotorControl m; m.arm(0); m.start(Direction::Left, 0); m.setTarget(10);
  m.tick(30); m.tick(60); m.tick(90);
  if (m.current() != 3 || !m.slowStop(90)) return false;
  if (m.arm(90) || m.start(Direction::Right, 90) || m.setTarget(10)) return false;
  m.tick(120); if (m.current() != 2) return false;
  m.slowStop(120); m.tick(150); m.tick(180);
  if (m.current() || m.state() != State::AwaitStandstill) return false;
  if (m.start(Direction::Right, 180)) return false;
  return m.arm(180) && m.start(Direction::Right, 180) && m.target() == 0;
}
constexpr bool checkEmergencyLatch() {
  MotorControl m; m.arm(0); m.start(Direction::Right, 0); m.setTarget(300); m.tick(30);
  m.trip(Reason::Emergency);
  if (m.current() || m.target() || m.direction() != Direction::None) return false;
  if (m.heartbeat(31) || m.setTarget(300) || m.start(Direction::Left, 31)) return false;
  m.tick(10000);
  return m.current() == 0 && m.state() == State::Locked && m.arm(10000) && m.target() == 0;
}
constexpr bool checkLeaseAndRollover() {
  MotorControl m; constexpr uint32_t start = 0xfffffff0u;
  m.arm(start); m.start(Direction::Right, start); m.setTarget(10);
  m.tick(14); if (m.current() != 1) return false;
  m.tick(1983); if (!m.leaseActive()) return false;
  m.tick(1984);
  return m.current() == 0 && m.reason() == Reason::ConnectionLost && !m.heartbeat(1984) && !m.start(Direction::Left, 1984);
}
constexpr bool checkKeepaliveAndFault() {
  MotorControl m; m.arm(0); m.start(Direction::Right, 0); m.setTarget(10);
  m.tick(30); if (!m.heartbeat(1000)) return false;
  m.tick(2500); if (!m.leaseActive()) return false;
  m.trip(Reason::OtaError, true);
  m.trip(Reason::Emergency);
  return m.current() == 0 && m.state() == State::Fault && !m.arm(2500);
}
constexpr bool checkFullRamp() {
  MotorControl m; m.arm(0); m.start(Direction::Right, 0); m.setTarget(300);
  for (uint32_t now = 30; now <= 12000; now += 30) {
    m.heartbeat(now); m.tick(now);
    if (m.current() > 300) return false;
  }
  if (m.current() != 300) return false;
  m.slowStop(12000);
  for (uint32_t now = 12030; now <= 21000; now += 30) { m.heartbeat(now); m.tick(now); }
  return m.current() == 0 && m.target() == 0 && m.state() == State::AwaitStandstill;
}
static_assert(checkBootAndStart(), "Boot/start must remain zero and reject reversal");
static_assert(checkRampAndLimits(), "PWM limit or maximum ramp step violated");
static_assert(checkGradualStop(), "Stopping/reversal requires standstill confirmation");
static_assert(checkEmergencyLatch(), "Emergency stop must latch and reject stale commands");
static_assert(checkLeaseAndRollover(), "Expired heartbeat or millis rollover failed");
static_assert(checkKeepaliveAndFault(), "Heartbeat or OTA fault latch failed");
static_assert(checkFullRamp(), "Full acceleration/deceleration or PWM limit failed");
