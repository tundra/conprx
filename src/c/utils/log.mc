;// Generic events for logging from the agent.

LanguageNames=(English=0x409:MSG00409)

SeverityNames=(
  Verbose=0x0:STATUS_SEVERITY_SUCCESS
  Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
  Warning=0x2:STATUS_SEVERITY_WARNING
  Error=0x3:STATUS_SEVERITY_ERROR
)

;// --- Categories ---

MessageIdTypedef=WORD

MessageId=0x1
SymbolicName=GENERIC_CATEGORY
Language=English
All Events
.

;// --- Events ---

MessageIdTypedef=DWORD

MessageId=0x2
Severity=Verbose
Facility=Application
SymbolicName=MSG_LOG_VERBOSE
Language=English
INFO %1:%2: %3
.

MessageId=0x3
Severity=Informational
Facility=Application
SymbolicName=MSG_LOG_INFO
Language=English
INFO %1:%2: %3
.

MessageId=0x4
Severity=Warning
Facility=Application
SymbolicName=MSG_LOG_WARNING
Language=English
WARN %1:%2: %3
.

MessageId=0x5
Severity=Error
Facility=Application
SymbolicName=MSG_LOG_ERROR
Language=English
ERROR %1:%2: %3
.
