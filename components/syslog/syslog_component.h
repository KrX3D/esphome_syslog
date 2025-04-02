#pragma once
#ifndef SYSLOG_COMPONENT_H_0504CB6C_15D8_4AB4_A04C_8AF9063B737F
#define SYSLOG_COMPONENT_H_0504CB6C_15D8_4AB4_A04C_8AF9063B737F
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include <Syslog.h>
#include <Udp.h>
#if defined ESP8266 || defined ARDUINO_ESP8266_ESP01
    #include <ESP8266WiFi.h>
#else
    #include <WiFi.h>
#endif
#include <WiFiUdp.h>
#include <unordered_map>
#include <set>

namespace esphome {
namespace syslog {

struct SYSLOGSettings {
    std::string address;
    uint16_t port;
    std::string client_id;
    int min_log_level;
};

class SyslogComponent : public Component {
    public:
        SyslogComponent();
        float get_setup_priority() const override;
        void setup() override;
        void loop() override;
        
        // Runtime changeable settings
        void set_server_ip(const std::string &address);
        const std::string &get_server_ip() const { return this->settings_.address; }
        
        void set_server_port(uint16_t port);
        uint16_t get_server_port() const { return this->settings_.port; }
        
        void set_client_id(const std::string &client_id) { this->settings_.client_id = client_id; }
        
        void set_min_log_level(int log_level) { this->settings_.min_log_level = log_level; }
        int get_min_log_level() const { return this->settings_.min_log_level; }
        
        void set_enable_logger_messages(bool en);
        bool get_enable_logger_messages() const { return this->enable_logger; }
        
        void set_strip_colors(bool strip_colors);
        bool get_strip_colors() const { return this->strip_colors; }

        // Filter management
        void set_filter_mode(bool include_mode) { this->filter_include_mode = include_mode; }
        bool get_filter_mode() const { return this->filter_include_mode; }
        
        void clear_filters() { this->tag_filters.clear(); }
        void add_filter(const std::string &tag);
        void remove_filter(const std::string &tag);
        bool has_filter(const std::string &tag) const;
        std::vector<std::string> get_filters() const;
        
        void log(uint8_t level, const std::string &tag, const std::string &payload);
        
        // Helper method to extract component name from the tag
        std::string extract_component_name(const std::string &tag);
        
        // Method to check if a tag should be filtered
        bool should_send_log(const std::string &tag);

    protected:
        bool strip_colors;
        bool enable_logger;
        bool filter_include_mode; // true = include only these tags, false = exclude these tags
        std::set<std::string> tag_filters;
        SYSLOGSettings settings_;
        WiFiUDP *udp_ = NULL;
        bool recreate_udp();
};

// Custom action to log a message
template<typename... Ts> class SyslogLogAction : public Action<Ts...> {
    public:
        SyslogLogAction(SyslogComponent *parent) : parent_(parent) {}
        TEMPLATABLE_VALUE(uint8_t, level)
        TEMPLATABLE_VALUE(std::string, tag)
        TEMPLATABLE_VALUE(std::string, payload)
        void play(Ts... x) override {
            this->parent_->log(this->level_.value(x...), this->tag_.value(x...), this->payload_.value(x...));
        }
    protected:
        SyslogComponent *parent_;
};

// Custom action to add a tag filter
template<typename... Ts> class SyslogAddFilterAction : public Action<Ts...> {
public:
    SyslogAddFilterAction(SyslogComponent *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(std::string, tag)
    void play(Ts... x) override {
        this->parent_->add_filter(this->tag_.value(x...));
    }
protected:
    SyslogComponent *parent_;
};

// Custom action to remove a tag filter
template<typename... Ts> class SyslogRemoveFilterAction : public Action<Ts...> {
public:
    SyslogRemoveFilterAction(SyslogComponent *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(std::string, tag)
    void play(Ts... x) override {
        this->parent_->remove_filter(this->tag_.value(x...));
    }
protected:
    SyslogComponent *parent_;
};

// Custom action to clear all filters
template<typename... Ts> class SyslogClearFiltersAction : public Action<Ts...> {
public:
    SyslogClearFiltersAction(SyslogComponent *parent) : parent_(parent) {}
    void play(Ts... x) override {
        this->parent_->clear_filters();
    }
protected:
    SyslogComponent *parent_;
};

}  // namespace syslog
}  // namespace esphome
#endif
