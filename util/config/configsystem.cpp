#include "configsystem.h"
#include <iostream>
#include "../notification/notification.h"
#include "../../util/globals.h"

ConfigSystem::ConfigSystem() {
    char* appdata_path;
    size_t len;
    _dupenv_s(&appdata_path, &len, "APPDATA");

    if (appdata_path) {
        config_directory = std::string(appdata_path) + "\\AkvariumMacro\\config";
        free(appdata_path);
    }

    if (!fs::exists(config_directory)) {
        fs::create_directories(config_directory);
    }

    refresh_config_list();
}

void ConfigSystem::refresh_config_list() {
    config_files.clear();

    if (fs::exists(config_directory)) {
        for (const auto& entry : fs::directory_iterator(config_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".akv") {
                config_files.push_back(entry.path().stem().string());
            }
        }
    }
}

bool ConfigSystem::save_config(const std::string& name) {
    if (name.empty()) return false;

    json config_json;

    // === FEATURES ===
    std::cout << "[FEATURES] Saving features..." << "\n";
    auto& feat = config_json["features"];

    // AD Clicker
    feat["ADclick"] = globals::features::ADclick;
    feat["ADclickDelay"] = globals::features::ADclickDelay;
    feat["ADclickOnKey"] = globals::features::ADclickOnKey;
    feat["ADKeyToggle"] = globals::features::ADKeyToggle;

    feat["keybinds"]["ADclickBind"]["key"] = globals::features::ADclickBind.key;
    feat["keybinds"]["ADclickBind"]["type"] = static_cast<int>(globals::features::ADclickBind.type);
    feat["keybinds"]["ADkeyToggleBind"]["key"] = globals::features::ADkeyToggleBind.key;
    feat["keybinds"]["ADkeyToggleBind"]["type"] = static_cast<int>(globals::features::ADkeyToggleBind.type);

    // Indicators
    feat["AdIndicator"] = globals::features::AdIndicator;
    feat["AdIndicatorColor"][0] = globals::features::AdIndicatorColor.x;
    feat["AdIndicatorColor"][1] = globals::features::AdIndicatorColor.y;
    feat["AdIndicatorColor"][2] = globals::features::AdIndicatorColor.z;
    feat["AdIndicatorColor"][3] = globals::features::AdIndicatorColor.w;

    feat["attributeSwapIndicator"] = globals::features::attributeSwapIndicator;
    feat["attributeSwapIndicatorColor"][0] = globals::features::attributeSwapIndicatorColor.x;
    feat["attributeSwapIndicatorColor"][1] = globals::features::attributeSwapIndicatorColor.y;
    feat["attributeSwapIndicatorColor"][2] = globals::features::attributeSwapIndicatorColor.z;
    feat["attributeSwapIndicatorColor"][3] = globals::features::attributeSwapIndicatorColor.w;

    feat["spearSwapIndicator"] = globals::features::spearSwapIndicator;
    feat["spearSwapIndicatorColor"][0] = globals::features::spearSwapIndicatorColor.x;
    feat["spearSwapIndicatorColor"][1] = globals::features::spearSwapIndicatorColor.y;
    feat["spearSwapIndicatorColor"][2] = globals::features::spearSwapIndicatorColor.z;
    feat["spearSwapIndicatorColor"][3] = globals::features::spearSwapIndicatorColor.w;

    // Swap after click
    feat["swapOnClick"] = globals::features::swapOnClick;
    feat["swapDelay"] = globals::features::swapDelay;
    feat["keybinds"]["SwapOnTargetSlot"]["key"] = globals::features::SwapOnTargetSlot.key;
    feat["keybinds"]["SwapOnTargetSlot"]["type"] = static_cast<int>(globals::features::SwapOnTargetSlot.type);

    // Swap before click
    feat["swapBeforeClick"] = globals::features::swapBeforeClick;
    feat["keybinds"]["SwapBeforeTargetSlot"]["key"] = globals::features::SwapBeforeTargetSlot.key;
    feat["keybinds"]["SwapBeforeTargetSlot"]["type"] = static_cast<int>(globals::features::SwapBeforeTargetSlot.type);

    // Swap between clicks
    feat["swapBetweenClicks"] = globals::features::swapBetweenClicks;
    feat["keybinds"]["SwapBetweenTargetSlot"]["key"] = globals::features::SwapBetweenTargetSlot.key;
    feat["keybinds"]["SwapBetweenTargetSlot"]["type"] = static_cast<int>(globals::features::SwapBetweenTargetSlot.type);

    // Attribute swap
    feat["attributeSwap"] = globals::features::attributeSwap;
    feat["attributeSwapDelay"] = globals::features::attributeSwapDelay;
    feat["keybinds"]["attributeSwapTargetSlot"]["key"] = globals::features::attributeSwapTargetSlot.key;
    feat["keybinds"]["attributeSwapTargetSlot"]["type"] = static_cast<int>(globals::features::attributeSwapTargetSlot.type);
    feat["keybinds"]["attributeSwapKey"]["key"] = globals::features::attributeSwapKey.key;
    feat["keybinds"]["attributeSwapKey"]["type"] = static_cast<int>(globals::features::attributeSwapKey.type);

    // Spear swap
    feat["spearSwap"] = globals::features::spearSwap;
    feat["keybinds"]["spearSwapTargetSlot"]["key"] = globals::features::spearSwapTargetSlot.key;
    feat["keybinds"]["spearSwapTargetSlot"]["type"] = static_cast<int>(globals::features::spearSwapTargetSlot.type);
    feat["keybinds"]["spearSwapKey"]["key"] = globals::features::spearSwapKey.key;
    feat["keybinds"]["spearSwapKey"]["type"] = static_cast<int>(globals::features::spearSwapKey.type);

    // === MISC ===
    std::cout << "[MISC] Saving misc..." << "\n";
    auto& misc = config_json["misc"];

    misc["watermark"] = globals::misc::watermark;
    misc["vsync"] = globals::misc::vsync;
    misc["keybinds"] = globals::misc::keybinds;
    misc["keybindsstyle"] = globals::misc::keybindsstyle;
    misc["streamproof"] = globals::misc::streamproof;
    misc["panicKey"] = globals::misc::panicKey;
    misc["keybinds_data"]["panicKeyBind"]["key"] = globals::misc::panicKeyBind.key;
    misc["keybinds_data"]["panicKeyBind"]["type"] = static_cast<int>(globals::misc::panicKeyBind.type);

    if (globals::misc::watermarkstuff != nullptr) {
        misc["watermarkstuff"] = *globals::misc::watermarkstuff;
    }

    std::string filepath = config_directory + "\\" + name + ".akv";
    std::ofstream file(filepath);

    if (file.is_open()) {
        file << config_json.dump(2);
        file.close();
        refresh_config_list();
        current_config_name = name;
        save_last_config_name(name);
        std::cout << "[CONFIG] Successfully saved config: " << name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Failed to save config: " << name << "\n";
    return false;
}

bool ConfigSystem::load_config(const std::string& name) {
    if (name.empty()) return false;

    std::string filepath = config_directory + "\\" + name + ".akv";
    std::ifstream file(filepath);

    if (file.is_open()) {
        try {
            json config_json;
            file >> config_json;
            file.close();

            std::cout << "[CONFIG] Loading config: " << name << "\n";

            // helper lambdas
            auto load_keybind = [](const json& j, const std::string& key, keybind& bind) {
                if (j.contains(key)) {
                    if (j[key].contains("key"))  bind.key = j[key]["key"].get<int>();
                    if (j[key].contains("type")) bind.type = static_cast<keybind::c_keybind_type>(j[key]["type"].get<int>());
                }
                };
            auto load_color = [](const json& j, const std::string& key, ImVec4& color) {
                if (j.contains(key) && j[key].is_array() && j[key].size() == 4) {
                    color.x = j[key][0]; color.y = j[key][1];
                    color.z = j[key][2]; color.w = j[key][3];
                }
                };

            // === FEATURES ===
            if (config_json.contains("features")) {
                std::cout << "[FEATURES] Loading features..." << "\n";
                auto& feat = config_json["features"];

                // AD Clicker
                if (feat.contains("ADclick"))      globals::features::ADclick = feat["ADclick"];
                if (feat.contains("ADclickDelay")) globals::features::ADclickDelay = feat["ADclickDelay"];
                if (feat.contains("ADclickOnKey")) globals::features::ADclickOnKey = feat["ADclickOnKey"];
                if (feat.contains("ADKeyToggle"))  globals::features::ADKeyToggle = feat["ADKeyToggle"];

                if (feat.contains("keybinds")) {
                    auto& kb = feat["keybinds"];
                    load_keybind(kb, "ADclickBind", globals::features::ADclickBind);
                    load_keybind(kb, "ADkeyToggleBind", globals::features::ADkeyToggleBind);
                    load_keybind(kb, "SwapOnTargetSlot", globals::features::SwapOnTargetSlot);
                    load_keybind(kb, "SwapBeforeTargetSlot", globals::features::SwapBeforeTargetSlot);
                    load_keybind(kb, "SwapBetweenTargetSlot", globals::features::SwapBetweenTargetSlot);
                    load_keybind(kb, "attributeSwapTargetSlot", globals::features::attributeSwapTargetSlot);
                    load_keybind(kb, "attributeSwapKey", globals::features::attributeSwapKey);
                    load_keybind(kb, "spearSwapTargetSlot", globals::features::spearSwapTargetSlot);
                    load_keybind(kb, "spearSwapKey", globals::features::spearSwapKey);
                }

                // Indicators
                if (feat.contains("AdIndicator"))            globals::features::AdIndicator = feat["AdIndicator"];
                if (feat.contains("attributeSwapIndicator")) globals::features::attributeSwapIndicator = feat["attributeSwapIndicator"];
                if (feat.contains("spearSwapIndicator"))     globals::features::spearSwapIndicator = feat["spearSwapIndicator"];
                load_color(feat, "AdIndicatorColor", globals::features::AdIndicatorColor);
                load_color(feat, "attributeSwapIndicatorColor", globals::features::attributeSwapIndicatorColor);
                load_color(feat, "spearSwapIndicatorColor", globals::features::spearSwapIndicatorColor);

                // Swaps
                if (feat.contains("swapOnClick"))      globals::features::swapOnClick = feat["swapOnClick"];
                if (feat.contains("swapBeforeClick"))  globals::features::swapBeforeClick = feat["swapBeforeClick"];
                if (feat.contains("swapBetweenClicks"))globals::features::swapBetweenClicks = feat["swapBetweenClicks"];
                if (feat.contains("swapDelay"))        globals::features::swapDelay = feat["swapDelay"];

                if (feat.contains("attributeSwap"))      globals::features::attributeSwap = feat["attributeSwap"];
                if (feat.contains("attributeSwapDelay")) globals::features::attributeSwapDelay = feat["attributeSwapDelay"];

                if (feat.contains("spearSwap")) globals::features::spearSwap = feat["spearSwap"];

                std::cout << "[FEATURES] Features loaded successfully" << "\n";
            }

            // === MISC ===
            if (config_json.contains("misc")) {
                std::cout << "[MISC] Loading misc..." << "\n";
                auto& misc = config_json["misc"];

                if (misc.contains("watermark"))     globals::misc::watermark = misc["watermark"];
                if (misc.contains("vsync"))         globals::misc::vsync = misc["vsync"];
                if (misc.contains("keybinds"))      globals::misc::keybinds = misc["keybinds"];
                if (misc.contains("keybindsstyle")) globals::misc::keybindsstyle = misc["keybindsstyle"];
                if (misc.contains("streamproof"))   globals::misc::streamproof = misc["streamproof"];
                if (misc.contains("panicKey"))      globals::misc::panicKey = misc["panicKey"];

                if (misc.contains("keybinds_data")) {
                    auto& kb = misc["keybinds_data"];
                    if (kb.contains("panicKeyBind")) {
                        if (kb["panicKeyBind"].contains("key"))  globals::misc::panicKeyBind.key = kb["panicKeyBind"]["key"];
                        if (kb["panicKeyBind"].contains("type")) globals::misc::panicKeyBind.type = static_cast<keybind::c_keybind_type>(kb["panicKeyBind"]["type"].get<int>());
                    }
                }

                if (misc.contains("watermarkstuff") && misc["watermarkstuff"].is_array()) {
                    if (globals::misc::watermarkstuff == nullptr)
                        globals::misc::watermarkstuff = new std::vector<int>();
                    *globals::misc::watermarkstuff = misc["watermarkstuff"].get<std::vector<int>>();
                }

                std::cout << "[MISC] Misc loaded successfully" << "\n";
            }

            current_config_name = name;
            save_last_config_name(name);
            std::cout << "[CONFIG] Successfully loaded config: " << name << "\n";
            return true;
        }
        catch (const json::parse_error& e) {
            std::cout << "[CONFIG] Failed to parse config file: " << e.what() << "\n";
            file.close();
            return false;
        }
    }

    std::cout << "[CONFIG] Failed to open config file: " << name << "\n";
    return false;
}

bool ConfigSystem::delete_config(const std::string& name) {
    if (name.empty()) return false;

    std::string filepath = config_directory + "\\" + name + ".akv";

    if (fs::exists(filepath)) {
        fs::remove(filepath);
        refresh_config_list();

        if (current_config_name == name) {
            current_config_name.clear();
        }

        std::cout << "[CONFIG] Successfully deleted config: " << name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Config file not found: " << name << "\n";
    return false;
}

void ConfigSystem::render_config_ui(float width, float height) {
    ImAdd::BeginChild("CONFIG SYSTEM", ImVec2(width, height));
    {
        ImGui::Text("Config Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##config_name", config_name_buffer, sizeof(config_name_buffer));
        ImGui::Spacing();

        ImGui::Columns(3, nullptr, false);

        if (ImAdd::Button("Save Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Save button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                if (save_config(std::string(config_name_buffer))) {
                    Notifications::Success("Config '" + std::string(config_name_buffer) + "' saved successfully!");
                    memset(config_name_buffer, 0, sizeof(config_name_buffer));
                }
                else {
                    Notifications::Error("Failed to save config '" + std::string(config_name_buffer) + "'!");
                }
            }
            else {
                Notifications::Warning("Please enter a config name!");
            }
        }

        ImGui::NextColumn();

        if (ImAdd::Button("Load Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Load button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                if (load_config(std::string(config_name_buffer))) {
                    Notifications::Success("Config '" + std::string(config_name_buffer) + "' loaded successfully!");
                }
                else {
                    Notifications::Error("Failed to load config '" + std::string(config_name_buffer) + "'!");
                }
            }
            else {
                Notifications::Warning("Please enter a config name or select from list!");
            }
        }

        ImGui::NextColumn();

        if (ImAdd::Button("Delete Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Delete button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                std::string config_name = std::string(config_name_buffer);
                if (delete_config(config_name)) {
                    Notifications::Success("Config was deleted!");
                    memset(config_name_buffer, 0, sizeof(config_name_buffer));
                }
                else {
                    Notifications::Error("Failed To Delete!");
                }
            }
            else {
                Notifications::Warning("Select A Config!");
            }
        }

        ImGui::Columns(1);
        ImGui::Spacing();

        if (!config_files.empty()) {
            ImGui::Text("Available Configs:");
            ImGui::BeginChild("config_list", ImVec2(-1, -30));
            {
                for (const auto& config : config_files) {
                    bool is_selected = (current_config_name == config);

                    if (ImGui::Selectable(config.c_str(), is_selected)) {
                        std::cout << "[CONFIG] Selected config: " << config << "\n";
                        strcpy_s(config_name_buffer, sizeof(config_name_buffer), config.c_str());
                        Notifications::Info("Selected config: " + config, 2.0f);
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndChild();

            if (ImAdd::Button("Refresh List", ImVec2(-1, 25))) {
                std::cout << "[CONFIG] Refreshing config list..." << "\n";
                size_t old_count = config_files.size();
                refresh_config_list();
                size_t new_count = config_files.size();

                if (new_count != old_count) {
                    Notifications::Info("Config list refreshed - Found " + std::to_string(new_count) + " configs", 2.0f);
                }
                else {
                    Notifications::Info("Config list refreshed", 1.5f);
                }
            }
        }
        else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No configs found");

            ImGui::Spacing();
            ImGui::TextWrapped("Create your first config by entering a name above and clicking 'Save Config'.");
        }
    }
    ImAdd::EndChild();
}

const std::string& ConfigSystem::get_current_config() const {
    return current_config_name;
}

void ConfigSystem::save_last_config_name(const std::string& name) {
    std::string filepath = config_directory + "\\last.cfg";
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << name;
        file.close();
    }
}

std::string ConfigSystem::load_last_config_name() {
    std::string filepath = config_directory + "\\last.cfg";
    std::ifstream file(filepath);
    if (file.is_open()) {
        std::string name;
        std::getline(file, name);
        file.close();
        return name;
    }
    return "";
}