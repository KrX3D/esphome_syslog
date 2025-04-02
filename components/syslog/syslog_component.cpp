#include "syslog_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace syslog {

static const char *TAG = "syslog";
// https://github.com/arcao/Syslog/blob/master/src/Syslog.h#L37-44
// https://github.com/esphome/esphome/blob/5c86f332b269fd3e4bffcbdf3359a021419effdd/esphome/core/log.h#L19-26
static const uint8_t esphome_to_syslog_log_levels[] = {0, 3, 4, 6, 5, 7, 7, 7};

SyslogComponent::SyslogComponent() {
    this->settings_.client_id = App.get_name();
    this->filter_include_mode = false; // Default to exclude mode
    this->strip_colors = true;
    this->enable_logger = true;
    // Get the WifiUDP client here instead of getting it in setup() to make sure we always have a client when calling log()
    // Calling log() without the device connected should not be an issue since there is a wifi connected check and WifiUDP fails "silently" and doesn't generate an exception anyways
    this->udp_ = new WiFiUDP(); 
}

void SyslogComponent::setup() {
    this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", "Syslog started");
    ESP_LOGI(TAG, "Started");
    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
            if(!this->enable_logger || (level > this->settings_.min_log_level)) return;
            
            std::string tag_str(tag);
            if (!this->should_send_log(tag_str)) {
                return;
            }
            
            if(this->strip_colors) { //Strips the "033[0;xxx" at the beginning and the "#033[0m" at the end of log messages
                std::string org_msg(message);
                this->log(level, tag, org_msg.substr(7, org_msg.size() -7 -4));
            } else {
                this->log(level, tag, message);
            }
        });
    }
    #endif
}

void SyslogComponent::loop() {
}

bool SyslogComponent::recreate_udp() {
    if (this->udp_ != nullptr) {
        delete this->udp_;
    }
    this->udp_ = new WiFiUDP();
    return this->udp_ != nullptr;
}

void SyslogComponent::set_server_ip(const std::string &address) {
    if (this->settings_.address != address) {
        this->settings_.address = address;
        ESP_LOGI(TAG, "Syslog server IP updated to %s", address.c_str());
    }
}

void SyslogComponent::set_server_port(uint16_t port) {
    if (this->settings_.port != port) {
        this->settings_.port = port;
        ESP_LOGI(TAG, "Syslog server port updated to %d", port);
    }
}

void SyslogComponent::set_enable_logger_messages(bool en) {
    if (this->enable_logger != en) {
        this->enable_logger = en;
        ESP_LOGI(TAG, "Logger messages %s", en ? "enabled" : "disabled");
    }
}

void SyslogComponent::set_strip_colors(bool strip_colors) {
    if (this->strip_colors != strip_colors) {
        this->strip_colors = strip_colors;
        ESP_LOGI(TAG, "Strip colors %s", strip_colors ? "enabled" : "disabled");
    }
}

void SyslogComponent::add_filter(const std::string &tag) {
    this->tag_filters.insert(tag);
    ESP_LOGI(TAG, "Added filter for tag: %s", tag.c_str());
}

void SyslogComponent::remove_filter(const std::string &tag) {
    this->tag_filters.erase(tag);
    ESP_LOGI(TAG, "Removed filter for tag: %s", tag.c_str());
}

bool SyslogComponent::has_filter(const std::string &tag) const {
    return this->tag_filters.find(tag) != this->tag_filters.end();
}

std::vector<std::string> SyslogComponent::get_filters() const {
    std::vector<std::string> result;
    for (const auto &tag : this->tag_filters) {
        result.push_back(tag);
    }
    return result;
}

std::string SyslogComponent::extract_component_name(const std::string &tag) {
    size_t colon_pos = tag.find(':');
    if (colon_pos != std::string::npos) {
        return tag.substr(0, colon_pos);
    }
    return tag;
}

bool SyslogComponent::should_send_log(const std::string &tag) {
    // Extract component name from tag (before the colon)
    std::string component = extract_component_name(tag);
    
    // If no filters are set, always allow
    if (this->tag_filters.empty()) {
        return true;
    }
    
    bool has_component = this->has_filter(component);
    
    // In include mode, only send logs from components in the filter list
    // In exclude mode, send logs from all components EXCEPT those in the filter list
    if (this->filter_include_mode) {
        return has_component;
    } else {
        return !has_component;
    }
}

void SyslogComponent::log(uint8_t level, const std::string &tag, const std::string &payload) {
    level = level > 7 ? 7 : level;
    // Simple check to make sure that there is connectivity, if not, log the issue and return
    if(WiFi.status() != WL_CONNECTED) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but Wifi isn't connected yet", tag.c_str(), payload.c_str(), level);
        return;
    }
    
    Syslog syslog(
        *this->udp_,
        this->settings_.address.c_str(),
        this->settings_.port,
        this->settings_.client_id.c_str(),
        tag.c_str(),
        LOG_KERN
    );
    
    if(!syslog.log(esphome_to_syslog_log_levels[level], payload.c_str())) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but failed for an unknown reason", tag.c_str(), payload.c_str(), level);
    }
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::AFTER_WIFI;
}

}  // namespace syslog
}  // namespace esphome
