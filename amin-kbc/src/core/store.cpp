#include "store.h"

#include "json.h"

namespace core {

static const ClipEntry& empty_entry() {
    static const ClipEntry kEmpty;
    return kEmpty;
}

ClipboardStore::AddResult ClipboardStore::add(const u16& raw_text, int64_t ts) {
    u16 text = clean_clipboard_text(raw_text);
    if (is_blank(text)) return AddResult::kRejectedEmpty;
    if (text.size() > kMaxChars) return AddResult::kRejectedTooBig;

    // Exact duplicate? Drop the old copy first; the new one goes to the front so
    // the list always reflects "most recently copied".
    bool existed = false;
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (it->text == text) {
            items_.erase(it);
            existed = true;
            break;
        }
    }

    ClipEntry e;
    e.ts = ts;
    e.text = std::move(text);
    items_.push_front(std::move(e));

    while (items_.size() > kMaxEntries) items_.pop_back();

    return existed ? AddResult::kMovedToTop : AddResult::kAdded;
}

void ClipboardStore::clear() { items_.clear(); }

bool ClipboardStore::remove_at(size_t index) {
    if (index >= items_.size()) return false;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

const ClipEntry& ClipboardStore::at(size_t index) const {
    if (index >= items_.size()) return empty_entry();
    return items_[index];
}

std::vector<size_t> ClipboardStore::search(const u16& filter) const {
    std::vector<size_t> out;
    const u16 needle = trim(filter);
    out.reserve(items_.size());
    if (needle.empty()) {
        for (size_t i = 0; i < items_.size(); ++i) out.push_back(i);
        return out;
    }
    for (size_t i = 0; i < items_.size(); ++i) {
        if (contains_ci(items_[i].text, needle)) out.push_back(i);
    }
    return out;
}

std::string ClipboardStore::to_json() const {
    Json root = Json::make_object();
    root.set("app", Json::make_string("AMIN Keyboard & Clipboard"));
    root.set("format", Json::make_number(1));
    Json arr = Json::make_array();
    for (const auto& e : items_) {
        Json item = Json::make_object();
        item.set("t", Json::make_number(static_cast<double>(e.ts)));
        item.set("x", Json::make_string(u16_to_utf8(e.text)));
        arr.push_back(item);
    }
    root.set("entries", arr);
    return root.dump(true);
}

bool ClipboardStore::from_json(const std::string& json_text, std::string& err) {
    clear();
    Json root;
    if (!Json::parse(json_text, root, err)) return false;
    if (!root.is_object()) {
        err = "root is not an object";
        return false;
    }
    const Json* arr = root.find("entries");
    if (!arr || !arr->is_array()) {
        err = "missing 'entries' array";
        return false;
    }
    for (size_t i = 0; i < arr->size(); ++i) {
        const Json& item = arr->at(i);
        if (!item.is_object()) continue;
        const std::string x = item.get_string("x");
        if (x.empty()) continue;
        u16 text = utf8_to_u16(x);
        if (is_blank(text) || text.size() > kMaxChars) continue;
        ClipEntry e;
        e.ts = static_cast<int64_t>(item.get_number("t", 0.0));
        e.text = std::move(text);
        items_.push_back(std::move(e));
        if (items_.size() >= kMaxEntries) break;
    }
    return true;
}

}  // namespace core
