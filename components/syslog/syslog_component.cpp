#include "syslog_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
/*
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
*/

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <lwip/ip.h>
#define IPPROTO_UDP IP_PROTO_UDP
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
    this->enable_direct_logs = true;    // New: Enable direct logging by default
    this->globally_enabled = true;      // New: Enable component by default
    this->filter_string = "";           // Initialize empty filter string
}

void SyslogComponent::setup() {
    // If component is globally disabled, don't set up the socket
    if (!this->globally_enabled) {
        this->log(ESPHOME_LOG_LEVEL_ERROR, "syslog", "Syslog component is disabled, skipping setup");
        return;
    }

    // Close existing socket if it exists
    if (this->socket_) {
        this->socket_.reset();
    }

    /*
     * Older versions of socket::set_sockaddr() return bogus results
     * if trying to log to a Legacy IP address when IPv6 is enabled.
     * Fixed by https://github.com/esphome/esphome/pull/7196
     */
    this->server_socklen = 0;
    if (ESPHOME_VERSION_CODE >= VERSION_CODE(2024, 8, 0)) {
        this->server_socklen = socket::set_sockaddr((struct sockaddr *)&this->server, sizeof(this->server),
                                                    this->settings_.address, this->settings_.port);
    }
#if USE_NETWORK_IPV6
    else if (this->settings_.address.find(':') != std::string::npos) {
        auto *server6 = reinterpret_cast<sockaddr_in6 *>(&this->server);
        memset(server6, 0, sizeof(*server6));
        server6->sin6_family = AF_INET6;
        server6->sin6_port = htons(this->settings_.port);

        ip6_addr_t ip6;
        inet6_aton(this->settings_.address.c_str(), &ip6);
        memcpy(server6->sin6_addr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
        this->server_socklen = sizeof(*server6);
    }
#endif /* USE_NETWORK_IPV6 */
    else {
        auto *server4 = reinterpret_cast<sockaddr_in *>(&this->server);
        memset(server4, 0, sizeof(*server4));
        server4->sin_family = AF_INET;
        server4->sin_addr.s_addr = inet_addr(this->settings_.address.c_str());
        server4->sin_port = htons(this->settings_.port);
        this->server_socklen = sizeof(*server4);
    }
    if (!this->server_socklen) {
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Failed to parse server IP address '%s'", this->settings_.address.c_str());
        this->log(ESPHOME_LOG_LEVEL_ERROR, "syslog", std::string(buffer));
        this->mark_failed();
        return;
    }
    this->socket_ = socket::socket(this->server.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (!this->socket_) {
        this->log(ESPHOME_LOG_LEVEL_ERROR, "syslog", "Failed to create UDP socket");
        this->mark_failed();
        return;
    }
 
    this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", "------------------------ Syslog started ------------------------");
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "Started with server: %s -> %d",
             this->settings_.address.c_str(), this->settings_.port);
    this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
    
    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
             if(!this->globally_enabled || !this->enable_logger || (level > this->settings_.min_log_level)) return;
            
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

void SyslogComponent::set_server_ip(const std::string &address) {
    if (this->settings_.address != address) {
        // Store the new address
        std::string old_address = this->settings_.address;
        this->settings_.address = address;
        
        // Only attempt to recreate the socket if we're already set up
        if (this->globally_enabled && this->is_setup()) {
            // First close the existing socket if it exists
            if (this->socket_) {
                this->socket_.reset();
            }
            
            // Create a new socket with the updated address
            this->setup();
            
            // Log the change AFTER the socket is set up
            char buffer[150];
            snprintf(buffer, sizeof(buffer), "Syslog server IP updated: %s -> %s",
                    old_address.c_str(), address.c_str());
            this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        }
    }
}

void SyslogComponent::set_server_port(uint16_t port) {
    if (this->settings_.port != port) {
        // Store the old port for logging
        uint16_t old_port = this->settings_.port;
        this->settings_.port = port;
        
        // Only attempt to recreate the socket if we're already set up
        if (this->globally_enabled && this->is_setup()) {
            // First close the existing socket if it exists
            if (this->socket_) {
                this->socket_.reset();
            }
            
            // Create a new socket with the updated port
            this->setup();
            
            // Log the change AFTER the socket is set up
            char buffer[150];
            snprintf(buffer, sizeof(buffer), "Syslog server port updated: %d -> %d",
                    old_port, port);
            this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        }
    }
}

void SyslogComponent::set_enable_logger_messages(bool en) {
    if (this->enable_logger != en) { 
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Logger messages: %s -> %s",
                 this->enable_logger ? "enabled" : "disabled",
                 en ? "enabled" : "disabled");
        this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        this->enable_logger = en;
    }
}

void SyslogComponent::set_strip_colors(bool strip_colors) {
    if (this->strip_colors != strip_colors) {    
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Strip colors: %s -> %s",
                 this->strip_colors ? "enabled" : "disabled",
                 strip_colors ? "enabled" : "disabled");
        this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        this->strip_colors = strip_colors;
    }
}

void SyslogComponent::set_enable_direct_logs(bool en) {
    if (this->enable_direct_logs != en) {        
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Direct logging: %s -> %s",
                 this->enable_direct_logs ? "enabled" : "disabled",
                 en ? "enabled" : "disabled");
        this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        this->enable_direct_logs = en;
    }
}

void SyslogComponent::set_globally_enabled(bool en) {
    if (this->globally_enabled != en) {

        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Syslog component: %s -> %s",
                 this->globally_enabled ? "enabled" : "disabled",
                 en ? "enabled" : "disabled");
        this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
        
        this->globally_enabled = en;
        
        // If enabling, make sure to set up the socket
        if (en) {
            this->setup();
        } else {
            // If disabling, close any open socket
            if (this->socket_) {
                this->socket_.reset();
            }
        }
    }
}

void SyslogComponent::add_filter(const std::string &tag) {
    this->tag_filters.insert(tag);
    char buffer[150];
    snprintf(buffer, sizeof(buffer), "Added filter for tag: '%s'", tag.c_str());
    this->log(ESPHOME_LOG_LEVEL_ERROR, "syslog", std::string(buffer));
}

void SyslogComponent::remove_filter(const std::string &tag) {
    this->tag_filters.erase(tag);
    char buffer[150];
    snprintf(buffer, sizeof(buffer), "Removed filter for tag: '%s'", tag.c_str());
    this->log(ESPHOME_LOG_LEVEL_ERROR, "syslog", std::string(buffer));
}

void SyslogComponent::clear_filters() {
    this->tag_filters.clear();
    this->filter_string = "";
    
    // Update text sensor if available
    if (this->filter_string_text_ != nullptr) {
        this->filter_string_text_->publish_state("");
    }
    
    this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", "All filters cleared");
}

// Helper function to trim whitespace
std::string trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void SyslogComponent::set_filter_string(const std::string &filter_string) {
    if (this->filter_string != filter_string) {
        this->filter_string = filter_string;
        
        // Clear existing filters
        this->tag_filters.clear();
        
        // Parse the new filter string
        if (!filter_string.empty()) {
            size_t start = 0;
            size_t end = 0;
            
            while ((end = filter_string.find(',', start)) != std::string::npos) {
                std::string item = filter_string.substr(start, end - start);
                // Trim whitespace
                item = trim(item);
                
                if (!item.empty()) {
                    this->add_filter(item);
                }
                
                start = end + 1;
            }
            
            // Add the last item
            std::string item = filter_string.substr(start);
            // Trim whitespace
            item = trim(item);
            
            if (!item.empty()) {
                this->add_filter(item);
            }
        }
        
        // Update text sensor if available
        if (this->filter_string_text_ != nullptr) {
            this->filter_string_text_->publish_state(this->filter_string);
        }
        
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "Filter string updated: '%s'", filter_string.c_str());
        this->log(ESPHOME_LOG_LEVEL_INFO, "syslog", std::string(buffer));
    }
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
     // Check if component is enabled and if direct logs are allowed
     if (!this->globally_enabled || this->is_failed()) {
         return;
     }
     
     // For direct log calls, check the enable_direct_logs flag
     if (!this->enable_direct_logs && tag != "syslog") {
        return;
    }

    level = level > 7 ? 7 : level;
    
    if (!this->socket_) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but socket isn't connected", tag.c_str(), payload.c_str(), level);
        return;
    }
    
    int pri = esphome_to_syslog_log_levels[level];
    std::string buf = str_sprintf("<%d>1 - %s %s - - - \xEF\xBB\xBF%s",
                                  pri, this->settings_.client_id.c_str(),
                                  tag.c_str(), payload.c_str());
    if (this->socket_->sendto(buf.c_str(), buf.length(), 0, (struct sockaddr *)&this->server, this->server_socklen) < 0) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but failed for an unknown reason", tag.c_str(), payload.c_str(), level);
    }
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::AFTER_WIFI;
}

}  // namespace syslog
}  // namespace esphome
