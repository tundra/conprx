;// Generic events for logging from the agent.

LanguageNames=(English=0x409:MSG00409)

;// --- Categories ---

MessageIdTypedef=WORD

MessageId=0x1
SymbolicName=GENERIC_CATEGORY
Language=English
All Events
.

;// --- Events ---

MessageIdTypedef=DWORD

MessageId=0x1
Severity=Informational
Facility=Application
SymbolicName=MSG_LOG_INFO
Language=English
INFO %1:%2: %3
.

MessageId=0x2
Severity=Warning
Facility=Application
SymbolicName=MSG_LOG_WARNING
Language=English
WARN %1:%2: %3
.

MessageId=0x3
Severity=Error
Facility=Application
SymbolicName=MSG_LOG_ERROR
Language=English
ERROR %1:%2: %3
.
