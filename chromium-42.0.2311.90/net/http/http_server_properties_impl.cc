// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace net {

namespace {

const uint64 kBrokenAlternateProtocolDelaySecs = 300;

}  // namespace

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : spdy_servers_map_(SpdyServerHostPortMap::NO_AUTO_EVICT),
      alternate_protocol_map_(AlternateProtocolMap::NO_AUTO_EVICT),
      spdy_settings_map_(SpdySettingsMap::NO_AUTO_EVICT),
      server_network_stats_map_(ServerNetworkStatsMap::NO_AUTO_EVICT),
      alternate_protocol_probability_threshold_(1),
      weak_ptr_factory_(this) {
  canonical_suffixes_.push_back(".c.youtube.com");
  canonical_suffixes_.push_back(".googlevideo.com");
  canonical_suffixes_.push_back(".googleusercontent.com");
}

HttpServerPropertiesImpl::~HttpServerPropertiesImpl() {
}

void HttpServerPropertiesImpl::InitializeSpdyServers(
    std::vector<std::string>* spdy_servers,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (!spdy_servers)
    return;
  // Add the entries from persisted data.
  for (std::vector<std::string>::reverse_iterator it = spdy_servers->rbegin();
       it != spdy_servers->rend(); ++it) {
    spdy_servers_map_.Put(*it, support_spdy);
  }
}

void HttpServerPropertiesImpl::InitializeAlternateProtocolServers(
    AlternateProtocolMap* alternate_protocol_map) {
  // Keep all the broken ones since those don't get persisted.
  for (AlternateProtocolMap::iterator it = alternate_protocol_map_.begin();
       it != alternate_protocol_map_.end();) {
    AlternateProtocolMap::iterator old_it = it;
    ++it;
    if (!old_it->second.is_broken) {
      alternate_protocol_map_.Erase(old_it);
    }
  }

  // Add the entries from persisted data.
  for (AlternateProtocolMap::reverse_iterator it =
           alternate_protocol_map->rbegin();
       it != alternate_protocol_map->rend(); ++it) {
    alternate_protocol_map_.Put(it->first, it->second);
  }

  // Attempt to find canonical servers.
  uint16 canonical_ports[] = { 80, 443 };
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    for (size_t j = 0; j < arraysize(canonical_ports); ++j) {
      HostPortPair canonical_host(canonical_suffix, canonical_ports[j]);
      // If we already have a valid canonical server, we're done.
      if (ContainsKey(canonical_host_to_origin_map_, canonical_host) &&
          (alternate_protocol_map_.Peek(canonical_host_to_origin_map_[
               canonical_host]) != alternate_protocol_map_.end())) {
        continue;
      }
      // Now attempt to find a server which matches this origin and set it as
      // canonical .
      for (AlternateProtocolMap::const_iterator it =
               alternate_protocol_map_.begin();
           it != alternate_protocol_map_.end(); ++it) {
        if (EndsWith(it->first.host(), canonical_suffixes_[i], false)) {
          canonical_host_to_origin_map_[canonical_host] = it->first;
          break;
        }
      }
    }
  }
}

void HttpServerPropertiesImpl::InitializeSpdySettingsServers(
    SpdySettingsMap* spdy_settings_map) {
  for (SpdySettingsMap::reverse_iterator it = spdy_settings_map->rbegin();
       it != spdy_settings_map->rend(); ++it) {
    spdy_settings_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::InitializeSupportsQuic(
    IPAddressNumber* last_address) {
  if (last_address)
    last_quic_address_ = *last_address;
}

void HttpServerPropertiesImpl::InitializeServerNetworkStats(
    ServerNetworkStatsMap* server_network_stats_map) {
  for (ServerNetworkStatsMap::reverse_iterator it =
           server_network_stats_map->rbegin();
       it != server_network_stats_map->rend(); ++it) {
    server_network_stats_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::GetSpdyServerList(
    base::ListValue* spdy_server_list,
    size_t max_size) const {
  DCHECK(CalledOnValidThread());
  DCHECK(spdy_server_list);
  spdy_server_list->Clear();
  size_t count = 0;
  // Get the list of servers (host/port) that support SPDY.
  for (SpdyServerHostPortMap::const_iterator it = spdy_servers_map_.begin();
       it != spdy_servers_map_.end() && count < max_size; ++it) {
    const std::string spdy_server_host_port = it->first;
    if (it->second) {
      spdy_server_list->Append(new base::StringValue(spdy_server_host_port));
      ++count;
    }
  }
}

static const AlternateProtocolInfo* g_forced_alternate_protocol = NULL;

// static
void HttpServerPropertiesImpl::ForceAlternateProtocol(
    const AlternateProtocolInfo& info) {
  // Note: we're going to leak this.
  if (g_forced_alternate_protocol)
    delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = new AlternateProtocolInfo(info);
}

// static
void HttpServerPropertiesImpl::DisableForcedAlternateProtocol() {
  delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = NULL;
}

base::WeakPtr<HttpServerProperties> HttpServerPropertiesImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HttpServerPropertiesImpl::Clear() {
  DCHECK(CalledOnValidThread());
  spdy_servers_map_.Clear();
  alternate_protocol_map_.Clear();
  canonical_host_to_origin_map_.clear();
  spdy_settings_map_.Clear();
  last_quic_address_.clear();
  server_network_stats_map_.Clear();
}

bool HttpServerPropertiesImpl::SupportsRequestPriority(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(host_port_pair.ToString());
  if (spdy_host_port != spdy_servers_map_.end() && spdy_host_port->second)
    return true;

  const AlternateProtocolInfo info = GetAlternateProtocol(host_port_pair);
  return info.protocol == QUIC;
}

void HttpServerPropertiesImpl::SetSupportsSpdy(
    const HostPortPair& host_port_pair,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(host_port_pair.ToString());
  if ((spdy_host_port != spdy_servers_map_.end()) &&
      (spdy_host_port->second == support_spdy)) {
    return;
  }
  // Cache the data.
  spdy_servers_map_.Put(host_port_pair.ToString(), support_spdy);
}

bool HttpServerPropertiesImpl::RequiresHTTP11(
    const net::HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  return (http11_servers_.find(host_port_pair) != http11_servers_.end());
}

void HttpServerPropertiesImpl::SetHTTP11Required(
    const net::HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;

  http11_servers_.insert(host_port_pair);
}

void HttpServerPropertiesImpl::MaybeForceHTTP11(const HostPortPair& server,
                                                SSLConfig* ssl_config) {
  if (RequiresHTTP11(server)) {
    ForceHTTP11(ssl_config);
  }
}

std::string HttpServerPropertiesImpl::GetCanonicalSuffix(
    const std::string& host) {
  // If this host ends with a canonical suffix, then return the canonical
  // suffix.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(host, canonical_suffixes_[i], false)) {
      return canonical_suffix;
    }
  }
  return std::string();
}

AlternateProtocolInfo HttpServerPropertiesImpl::GetAlternateProtocol(
    const HostPortPair& server) {
  AlternateProtocolMap::const_iterator it =
      GetAlternateProtocolIterator(server);
  if (it != alternate_protocol_map_.end() &&
      it->second.probability >= alternate_protocol_probability_threshold_)
    return it->second;

  if (g_forced_alternate_protocol)
    return *g_forced_alternate_protocol;

  AlternateProtocolInfo uninitialized_alternate_protocol;
  return uninitialized_alternate_protocol;
}

void HttpServerPropertiesImpl::SetAlternateProtocol(
    const HostPortPair& server,
    uint16 alternate_port,
    AlternateProtocol alternate_protocol,
    double alternate_probability) {

  AlternateProtocolInfo alternate(alternate_port,
                                  alternate_protocol,
                                  alternate_probability);
  AlternateProtocolMap::const_iterator it =
      GetAlternateProtocolIterator(server);
  if (it != alternate_protocol_map_.end()) {
    const AlternateProtocolInfo existing_alternate = it->second;

    if (existing_alternate.is_broken) {
      DVLOG(1) << "Ignore alternate protocol since it's known to be broken.";
      return;
    }

    if (!existing_alternate.Equals(alternate)) {
      LOG(WARNING) << "Changing the alternate protocol for: "
                   << server.ToString()
                   << " from [Port: " << existing_alternate.port
                   << ", Protocol: " << existing_alternate.protocol
                   << ", Probability: " << existing_alternate.probability
                   << "] to [Port: " << alternate_port
                   << ", Protocol: " << alternate_protocol
                   << ", Probability: " << alternate_probability
                   << "].";
    }
  } else {
    if (alternate_probability >= alternate_protocol_probability_threshold_) {
      // TODO(rch): Consider the case where multiple requests are started
      // before the first completes. In this case, only one of the jobs
      // would reach this code, whereas all of them should should have.
      HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING);
    }
  }

  alternate_protocol_map_.Put(server, alternate);

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, server.port());
      canonical_host_to_origin_map_[canonical_host] = server;
      break;
    }
  }
}

void HttpServerPropertiesImpl::SetBrokenAlternateProtocol(
    const HostPortPair& server) {
  AlternateProtocolMap::iterator it = alternate_protocol_map_.Get(server);
  const AlternateProtocolInfo alternate = GetAlternateProtocol(server);
  if (it == alternate_protocol_map_.end()) {
    if (alternate.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL) {
      LOG(DFATAL) << "Trying to mark unknown alternate protocol broken.";
      return;
    }
    // This server's alternate protocol information is coming from a canonical
    // server. Add an entry in the map for this server explicitly so that
    // it can be marked as broken.
    it = alternate_protocol_map_.Put(server, alternate);
  }
  it->second.is_broken = true;
  const BrokenAlternateProtocolEntry entry(server, alternate.port,
                                           alternate.protocol);
  int count = ++broken_alternate_protocol_map_[entry];
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kBrokenAlternateProtocolDelaySecs);
  base::TimeTicks when = base::TimeTicks::Now() + delay * (1 << (count - 1));
  broken_alternate_protocol_list_.push_back(
      BrokenAlternateProtocolEntryWithTime(entry, when));

  // Do not leave this host as canonical so that we don't infer the other
  // hosts are also broken without testing them first.
  RemoveCanonicalHost(server);

  // If this is the only entry in the list, schedule an expiration task.
  // Otherwise it will be rescheduled automatically when the pending task runs.
  if (broken_alternate_protocol_list_.size() == 1) {
    ScheduleBrokenAlternateProtocolMappingsExpiration();
  }
}

bool HttpServerPropertiesImpl::WasAlternateProtocolRecentlyBroken(
    const HostPortPair& server) {
  const AlternateProtocolInfo alternate_protocol = GetAlternateProtocol(server);
  if (alternate_protocol.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return false;
  const BrokenAlternateProtocolEntry entry(server, alternate_protocol.port,
                                           alternate_protocol.protocol);
  return ContainsKey(broken_alternate_protocol_map_, entry);
}

void HttpServerPropertiesImpl::ConfirmAlternateProtocol(
    const HostPortPair& server) {
  const AlternateProtocolInfo alternate_protocol = GetAlternateProtocol(server);
  if (alternate_protocol.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return;
  const BrokenAlternateProtocolEntry entry(server, alternate_protocol.port,
                                           alternate_protocol.protocol);
  broken_alternate_protocol_map_.erase(entry);
}

void HttpServerPropertiesImpl::ClearAlternateProtocol(
    const HostPortPair& server) {
  AlternateProtocolMap::iterator it = alternate_protocol_map_.Peek(server);
  if (it != alternate_protocol_map_.end())
    alternate_protocol_map_.Erase(it);

  RemoveCanonicalHost(server);
}

const AlternateProtocolMap&
HttpServerPropertiesImpl::alternate_protocol_map() const {
  return alternate_protocol_map_;
}

const SettingsMap& HttpServerPropertiesImpl::GetSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    CR_DEFINE_STATIC_LOCAL(SettingsMap, kEmptySettingsMap, ());
    return kEmptySettingsMap;
  }
  return it->second;
}

bool HttpServerPropertiesImpl::SetSpdySetting(
    const HostPortPair& host_port_pair,
    SpdySettingsIds id,
    SpdySettingsFlags flags,
    uint32 value) {
  if (!(flags & SETTINGS_FLAG_PLEASE_PERSIST))
      return false;

  SettingsFlagsAndValue flags_and_value(SETTINGS_FLAG_PERSISTED, value);
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    SettingsMap settings_map;
    settings_map[id] = flags_and_value;
    spdy_settings_map_.Put(host_port_pair, settings_map);
  } else {
    SettingsMap& settings_map = it->second;
    settings_map[id] = flags_and_value;
  }
  return true;
}

void HttpServerPropertiesImpl::ClearSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Peek(host_port_pair);
  if (it != spdy_settings_map_.end())
    spdy_settings_map_.Erase(it);
}

void HttpServerPropertiesImpl::ClearAllSpdySettings() {
  spdy_settings_map_.Clear();
}

const SpdySettingsMap&
HttpServerPropertiesImpl::spdy_settings_map() const {
  return spdy_settings_map_;
}

bool HttpServerPropertiesImpl::GetSupportsQuic(
    IPAddressNumber* last_address) const {
  if (last_quic_address_.empty())
    return false;

  *last_address = last_quic_address_;
  return true;
}

void HttpServerPropertiesImpl::SetSupportsQuic(bool used_quic,
                                               const IPAddressNumber& address) {
  if (!used_quic) {
    last_quic_address_.clear();
  } else {
    last_quic_address_ = address;
  }
}

void HttpServerPropertiesImpl::SetServerNetworkStats(
    const HostPortPair& host_port_pair,
    ServerNetworkStats stats) {
  server_network_stats_map_.Put(host_port_pair, stats);
}

const ServerNetworkStats* HttpServerPropertiesImpl::GetServerNetworkStats(
    const HostPortPair& host_port_pair) {
  ServerNetworkStatsMap::iterator it =
      server_network_stats_map_.Get(host_port_pair);
  if (it == server_network_stats_map_.end()) {
    return NULL;
  }
  return &it->second;
}

const ServerNetworkStatsMap&
HttpServerPropertiesImpl::server_network_stats_map() const {
  return server_network_stats_map_;
}

void HttpServerPropertiesImpl::SetAlternateProtocolProbabilityThreshold(
    double threshold) {
  alternate_protocol_probability_threshold_ = threshold;
}

AlternateProtocolMap::const_iterator
HttpServerPropertiesImpl::GetAlternateProtocolIterator(
    const HostPortPair& server) {
  AlternateProtocolMap::const_iterator it = alternate_protocol_map_.Get(server);
  if (it != alternate_protocol_map_.end())
    return it;

  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical != canonical_host_to_origin_map_.end())
    return alternate_protocol_map_.Get(canonical->second);

  return alternate_protocol_map_.end();
}

HttpServerPropertiesImpl::CanonicalHostMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalHost(HostPortPair server) const {
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, server.port());
      return canonical_host_to_origin_map_.find(canonical_host);
    }
  }

  return canonical_host_to_origin_map_.end();
}

void HttpServerPropertiesImpl::RemoveCanonicalHost(
    const HostPortPair& server) {
  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical == canonical_host_to_origin_map_.end())
    return;

  if (!canonical->second.Equals(server))
    return;

  canonical_host_to_origin_map_.erase(canonical->first);
}

void HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings() {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!broken_alternate_protocol_list_.empty()) {
    BrokenAlternateProtocolEntryWithTime entry_with_time =
        broken_alternate_protocol_list_.front();
    if (now < entry_with_time.when) {
      break;
    }

    const BrokenAlternateProtocolEntry& entry =
        entry_with_time.broken_alternate_protocol_entry;
    ClearAlternateProtocol(entry.server);
    broken_alternate_protocol_list_.pop_front();
  }
  ScheduleBrokenAlternateProtocolMappingsExpiration();
}

void
HttpServerPropertiesImpl::ScheduleBrokenAlternateProtocolMappingsExpiration() {
  if (broken_alternate_protocol_list_.empty()) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks when = broken_alternate_protocol_list_.front().when;
  base::TimeDelta delay = when > now ? when - now : base::TimeDelta();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings,
          weak_ptr_factory_.GetWeakPtr()),
      delay);
}

}  // namespace net
