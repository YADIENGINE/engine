#pragma once
#include <string>
#include "Terminal/TerminalTypes.h"

// UTF-8 text of one terminal cell, including combining marks.
[[nodiscard]] std::string TerminalCellText(const TerminalCell& cell);
