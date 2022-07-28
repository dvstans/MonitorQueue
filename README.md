# MonitorQueue
Atomic shared queue with work monitoring and recovery

This is a custom shared queue service that provides monitored, but non-durable,
message distribution (request/reply pattern) to worker processes. The main
differentiating feature of this service compared to other message queue systems
is its ability to track per-message processing progress and, in the event of 
excessive processing delay (i.e. worker failure), to auto-fail and re-queue messages
for assignment other workers. This approach differs from conventional worker process
monitoring by directly detecting stalled processing rather than simply checking for worker
livelines (which may not detect hung or failed internal threads). In
addition, failure monitoring is distributed across all worker processes to
eliminate the single point of failure risk of a central monitoring process.

Design trade-offs:
- This queuing approach requires that all messages be idempotent since messages
  may be processed more than once.
- The time required for completion of work units must be bounded as this bound
  forms the basis for failure assessment.
- Long-running work units must be broken into smaller units of woork that will be
  re-enqueued after each step is completed.
- No provision is made for persisting in-flight messages. The message publisher(s)
  and workers must ensure that messages states are persisted (if necessary)
- If the queue server itself fails, all in-flight messages are lost and
  the message publisher(s) must repopulate the queue based on current state (via
  a persistence service).
- If all worker processes fail simultaneously, no recovery is possible. An external
  process would need to monitor for this condition.
