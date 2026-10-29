// =============================================================================
//  AMIN Keyboard & Clipboard - core/store.h
//  In-memory clipboard history: newest first, max 100, exact de-duplication.
//  Pure logic, no Windows API: unit tested on the build host and via --selftest.
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "text.h"

namespace core {

struct ClipEntry {
    // Seconds since the Unix epoch (UTC). Only used for display and sorting.
    int64_t ts = 0;
    u16 text;
};

class ClipboardStore {
public:
    static const size_t kMaxEntries = 100;
    // Entries larger than this are not stored (keeps the history file small and
    // the UI responsive). 100k characters ~ 200 KB of UTF-16.
    static const size_t kMaxChars = 100000;

    enum class AddResult {
        kAdded,
        kMovedToTop,   // identical text already existed
        kRejectedEmpty,
        kRejectedTooBig
    };

    // Inserts at the front. Exact duplicates are removed and re-inserted at the
    // front (so the newest copy is always first). Returns what happened and,
    // when stored, the index of the entry (always 0 on success).
    AddResult add(const u16& raw_text, int64_t ts);

    void clear();
    bool remove_at(size_t index);

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    const ClipEntry& at(size_t index) const;
    const std::deque<ClipEntry>& entries() const { return items_; }

    // Indices (newest first) of entries containing the filter (case-insensitive
    // for ASCII, Persian letter variants folded). Empty filter -> all.
    std::vector<size_t> search(const u16& filter) const;

    int64_t newest_ts() const { return items_.empty() ? 0 : items_.front().ts; }

    // ---- Persistence -------------------------------------------------------
    std::string to_json() const;
    // Never throws; on malformed input the store is left empty and err is set.
    bool from_json(const std::string& json_text, std::string& err);

private:
    std::deque<ClipEntry> items_;  // items_[0] is the newest
};

}  // namespace core
