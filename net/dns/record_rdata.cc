// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

#include "net/base/big_endian.h"
#include "net/base/dns_util.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"

namespace net {

static const size_t kSrvRecordMinimumSize = 6;

RecordRdata::RecordRdata() {
}

SrvRecordRdata::SrvRecordRdata() : priority_(0), weight_(0), port_(0) {
}

SrvRecordRdata::~SrvRecordRdata() {}

// static
scoped_ptr<SrvRecordRdata> SrvRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (data.size() < kSrvRecordMinimumSize) return scoped_ptr<SrvRecordRdata>();

  scoped_ptr<SrvRecordRdata> rdata(new SrvRecordRdata);

  BigEndianReader reader(data.data(), data.size());
  // 2 bytes for priority, 2 bytes for weight, 2 bytes for port.
  reader.ReadU16(&rdata->priority_);
  reader.ReadU16(&rdata->weight_);
  reader.ReadU16(&rdata->port_);

  if (!parser.ReadName(data.substr(kSrvRecordMinimumSize).begin(),
                       &rdata->target_))
    return scoped_ptr<SrvRecordRdata>();

  return rdata.Pass();
}

uint16 SrvRecordRdata::Type() const {
  return SrvRecordRdata::kType;
}

bool SrvRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const SrvRecordRdata* srv_other = static_cast<const SrvRecordRdata*>(other);
  return weight_ == srv_other->weight_ &&
      port_ == srv_other->port_ &&
      priority_ == srv_other->priority_ &&
      target_ == srv_other->target_;
}

ARecordRdata::ARecordRdata() {
}

ARecordRdata::~ARecordRdata() {
}

// static
scoped_ptr<ARecordRdata> ARecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (data.size() != kIPv4AddressSize)
    return scoped_ptr<ARecordRdata>();

  scoped_ptr<ARecordRdata> rdata(new ARecordRdata);

  rdata->address_.resize(kIPv4AddressSize);
  for (unsigned i = 0; i < kIPv4AddressSize; ++i) {
    rdata->address_[i] = data[i];
  }

  return rdata.Pass();
}

uint16 ARecordRdata::Type() const {
  return ARecordRdata::kType;
}

bool ARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const ARecordRdata* a_other = static_cast<const ARecordRdata*>(other);
  return address_ == a_other->address_;
}

AAAARecordRdata::AAAARecordRdata() {
}

AAAARecordRdata::~AAAARecordRdata() {
}

// static
scoped_ptr<AAAARecordRdata> AAAARecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (data.size() != kIPv6AddressSize)
    return scoped_ptr<AAAARecordRdata>();

  scoped_ptr<AAAARecordRdata> rdata(new AAAARecordRdata);

  rdata->address_.resize(kIPv6AddressSize);
  for (unsigned i = 0; i < kIPv6AddressSize; ++i) {
    rdata->address_[i] = data[i];
  }

  return rdata.Pass();
}

uint16 AAAARecordRdata::Type() const {
  return AAAARecordRdata::kType;
}

bool AAAARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const AAAARecordRdata* a_other = static_cast<const AAAARecordRdata*>(other);
  return address_ == a_other->address_;
}

CnameRecordRdata::CnameRecordRdata() {
}

CnameRecordRdata::~CnameRecordRdata() {
}

// static
scoped_ptr<CnameRecordRdata> CnameRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  scoped_ptr<CnameRecordRdata> rdata(new CnameRecordRdata);

  if (!parser.ReadName(data.begin(), &rdata->cname_))
    return scoped_ptr<CnameRecordRdata>();

  return rdata.Pass();
}

uint16 CnameRecordRdata::Type() const {
  return CnameRecordRdata::kType;
}

bool CnameRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const CnameRecordRdata* cname_other =
      static_cast<const CnameRecordRdata*>(other);
  return cname_ == cname_other->cname_;
}

PtrRecordRdata::PtrRecordRdata() {
}

PtrRecordRdata::~PtrRecordRdata() {
}

// static
scoped_ptr<PtrRecordRdata> PtrRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  scoped_ptr<PtrRecordRdata> rdata(new PtrRecordRdata);

  if (!parser.ReadName(data.begin(), &rdata->ptrdomain_))
    return scoped_ptr<PtrRecordRdata>();

  return rdata.Pass();
}

uint16 PtrRecordRdata::Type() const {
  return PtrRecordRdata::kType;
}

bool PtrRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const PtrRecordRdata* ptr_other = static_cast<const PtrRecordRdata*>(other);
  return ptrdomain_ == ptr_other->ptrdomain_;
}

TxtRecordRdata::TxtRecordRdata() {
}

TxtRecordRdata::~TxtRecordRdata() {
}

// static
scoped_ptr<TxtRecordRdata> TxtRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  scoped_ptr<TxtRecordRdata> rdata(new TxtRecordRdata);

  for (size_t i = 0; i < data.size(); ) {
    uint8 length = data[i];

    if (i + length >= data.size())
      return scoped_ptr<TxtRecordRdata>();

    rdata->texts_.push_back(data.substr(i + 1, length).as_string());

    // Move to the next string.
    i += length + 1;
  }

  return rdata.Pass();
}

uint16 TxtRecordRdata::Type() const {
  return TxtRecordRdata::kType;
}

bool TxtRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const TxtRecordRdata* txt_other = static_cast<const TxtRecordRdata*>(other);
  return texts_ == txt_other->texts_;
}

}  // namespace net
