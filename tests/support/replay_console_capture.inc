// Included only by the replay executable, inside the application frame scope.
if (!options.smoke.empty() && player.coordinator()) {
    auto escape = [](const std::string &text) {
        std::string out;
        for (char c : text) {
            if (c == '\n')
                out += "\\n";
            else if (c == '\r')
                out += "\\r";
            else if (c == '\t')
                out += "\\t";
            else if (c == '\"' || c == '\\') {
                out += '\\';
                out += c;
            } else
                out += c;
        }
        return out;
    };
    static std::string previous;
    static std::size_t previous_events = std::size_t(-1);
    const auto &history = display.console_history().history();
    auto count = player.coordinator()->events().size();
    if (history != previous || count != previous_events) {
        std::ofstream out(options.smoke + "/console_trace.jsonl", std::ios::app);
        out << "{\"history\":\"" << escape(history) << "\",\"visible\":\""
            << escape(display.console_visible) << "\",\"speech\":" << scene.speech_visible()
            << ",\"typing\":" << display.console_typing << ",\"exposed\":" << console_exposed
            << ",\"events\":" << count
            << ",\"production_source\":" << (scene.human_console() ? scene.console_source() : 0)
            << ",\"production_prefix\":\""
            << escape(scene.human_console() ? scene.console_prefix() : std::string{}) << "\"}\n";
        previous = history;
        previous_events = count;
    }
    static std::set<std::uint64_t> sources;
    if (scene.human_console() && sources.insert(scene.console_source()).second) {
        std::ofstream out(options.smoke + "/console_production_sources.jsonl", std::ios::app);
        out << "{\"source\":" << scene.console_source() << ",\"lines\":[";
        bool first = true;
        for (const auto &line : scene.event().production_lines) {
            if (!first)
                out << ',';
            out << '\"' << escape(line) << '\"';
            first = false;
        }
        out << "]}\n";
    }
}
