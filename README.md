# MonitorQueue
Atomic shared queue with work monitoring and recovery

This is a Redis-based shared queue service that provides reliable, non-durable,
message distribution to worker processes. The main differentiating feature is the
ability to track per-message processing progress and, in the event of excessive
processing delay, invalidate and re-queue messages for assignment to another
worker. This approach differs from conventional worker process monitoring by
directly detecting stalled processing rather than simply checking for worker
livelines (which may not detect hung or failed threads, for example). In
addition, failure monitoring is distributed across all worker processes to
eliminate the single point of failure risk of a central monitoring process.

Design trade-offs:
- This queuing approach requires that all messages be idempotent since messages
  may be processed more than once.
- No provision is made for persisting in-flight messages. The message publisher(s)
  must ensure that queued messages are tracked
- If the queue system (Redis) itself fails, all in-flight messages are lost and
  the message publisher(s) must repopulate the queue based on current state (via
  a backend DB).
- If all worker processes fail simultaneously, no recovery is possible. An external
  process would need to monitor for this unlikely condition.
