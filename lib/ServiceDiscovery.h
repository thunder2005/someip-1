#pragma once

#include "SomeIP.h"
#include "SomeIP-common.h"
#include "boost/variant.hpp"
#include "Networking.h"

#include "serialization.h"

#include "SomeIP-log.h"

namespace SomeIP_Lib {

static const uint16_t SERVICE_DISCOVERY_UDP_PORT = 10102;

// TODO : What should be the broadcast address ?
static constexpr const char* SERVICE_DISCOVERY_MULTICAST_ADDRESS = "226.1.1.1";

struct Serializable {
	virtual ~Serializable() {
	}
	virtual void serialize(NetworkSerializer& serializer) const = 0;
};

class SomeIPServiceDiscoveryConfigurationOption : public Serializable {
public:
	enum class Type
		: uint8_t {
		IPv4Option = 0x04
	};

	virtual ~SomeIPServiceDiscoveryConfigurationOption() {
	}

	//	uint16_t m_length;
	Type m_type;
	///	uint8_t m_reserved;
};

class IPv4ConfigurationOption : public SomeIPServiceDiscoveryConfigurationOption {

public:
	IPv4ConfigurationOption(TransportProtocol protocol, IPV4Address address, TCPPort port) {
		m_type = Type::IPv4Option;
		m_protocol = protocol;
		m_port = port;
		m_address = address;
	}

	IPv4ConfigurationOption(NetworkDeserializer& deserializer) {
		m_type = Type::IPv4Option;
		std::array<uint8_t,4> address;
		deserializer >> address[0] >> address[1] >> address[2] >> address[3];
		deserializer >> reserved;
		deserializer.readEnum(m_protocol);
		deserializer >> m_port;
		m_address = address;
	}

	void serialize(NetworkSerializer& serializer) const override {
		auto address = m_address.toArray();
		serializer << address[0] << address[1] << address[2] << address[3];
		serializer << reserved;
		serializer.writeEnum(m_protocol);
		serializer << m_port;
	}

	IPV4Address m_address;
	uint16_t m_port = -1;
	TransportProtocol m_protocol;
	uint8_t reserved = 0;

};



class SomeIPServiceDiscoveryServiceEntry : public SomeIPServiceDiscoveryServiceEntryHeader, public Serializable {

public:
	SomeIPServiceDiscoveryServiceEntry() {
	}

	virtual ~SomeIPServiceDiscoveryServiceEntry() {
	}

	SomeIPServiceDiscoveryServiceEntry(SomeIPServiceDiscoveryEntryHeader::Type type, NetworkDeserializer& deserializer) {
		m_type = type;
		deserialize(deserializer);
	}

	void deserialize(NetworkDeserializer& deserializer) {
		deserializer >> m_indexFirstOptionRun >> m_indexSecondOptionRun >> to4BitsTuple(m_1,m_2) >> m_serviceID >> m_instanceID
		>> m_majorVersion >> LSBDeserializer<3, uint32_t>(m_ttl) >> m_minorVersion;
	}

	void serialize(NetworkSerializer& serializer) const override {
		serializer.writeEnum(m_type);
		serializer << m_indexFirstOptionRun << m_indexSecondOptionRun << to4BitsTuple(m_1, m_2) << m_serviceID << m_instanceID
			   << m_majorVersion << LSBSerializer<3, uint32_t>(m_ttl)  << m_minorVersion;
	}

};

class SomeIPServiceDiscoveryEventGroupEntry : public SomeIPServiceDiscoveryEventGroupEntryHeader, public Serializable {

public:
	SomeIPServiceDiscoveryEventGroupEntry() {
	}

	virtual ~SomeIPServiceDiscoveryEventGroupEntry() {
	}

	SomeIPServiceDiscoveryEventGroupEntry(SomeIPServiceDiscoveryEntryHeader::Type type, NetworkDeserializer& deserializer) {
		m_type = type;
		deserialize(deserializer);
	}

	void deserialize(NetworkDeserializer& deserializer) {
		Tuple4Bits4Bits tuple(m_1, m_2);
		deserializer >> m_indexFirstOptionRun >> m_indexSecondOptionRun >> to4BitsTuple(m_1, m_2) >> m_serviceID >> m_instanceID
		>> m_majorVersion >> LSBDeserializer<3, uint32_t>(m_ttl) >> m_reserved16 >> m_eventGroupID;
	}

	void serialize(NetworkSerializer& serializer) const override {
		serializer.writeEnum(m_type);
		serializer << m_indexFirstOptionRun << m_indexSecondOptionRun << to4BitsTuple(m_1, m_2) << m_serviceID << m_instanceID
			   << m_majorVersion << LSBSerializer<3, uint32_t>(m_ttl) << m_reserved16 << m_eventGroupID;
	}

};

typedef boost::variant<SomeIPServiceDiscoveryServiceEntry, SomeIPServiceDiscoveryEventGroupEntry> ServiceEntryVariant;
typedef boost::variant<IPv4ConfigurationOption> ConfigurationOptionVariant;

class SomeIPServiceDiscoveryMessage : public SomeIPServiceDiscoveryHeader, public Serializable {

	LOG_DECLARE_CLASS_CONTEXT("SISD", "SomeIPServiceDiscoveryMessage");

public:
	static const MessageID SERVICE_DISCOVERY_MEMBER_ID = 0xFFFF8100;

	SomeIPServiceDiscoveryMessage(NetworkDeserializer& deserializer) {
		SomeIPHeader::deserialize(deserializer);
		deserialize(deserializer);
	}

	virtual ~SomeIPServiceDiscoveryMessage() {
	}

	SomeIPServiceDiscoveryMessage(const SomeIPHeader& header, NetworkDeserializer& deserializer) {
		SomeIPHeader::operator=(header);
		deserialize(deserializer);
	}

	SomeIPServiceDiscoveryMessage(bool isMulticast) {
		setMessageID(SERVICE_DISCOVERY_MEMBER_ID);
		setMessageType(MessageType::NOTIFICATION);
		m_flags.unicast = !isMulticast;
		assignUniqueSessionID();
	}

	void assignUniqueSessionID() {
		auto& id = (m_flags.unicast ? s_nextUnicastSessionID : s_nextMulticastSessionID);
		auto& reboot = (m_flags.unicast ? s_UnicastReboot : s_MulticastReboot);
		m_flags.reboot = reboot;
		setRequestID(id++);
		if (id == 0) {
			id = 0x01;
			reboot = false;
		}
	}

	class SerializeVisitor : public boost::static_visitor<void> {
public:
		SerializeVisitor(NetworkSerializer& serializer) :
			m_serializer(serializer) {
		}

		template<typename TypeName>
		void operator()(const TypeName& param) const {
			param.serialize(m_serializer);
		}
		NetworkSerializer& m_serializer;
	};

	class OptionSerializeVisitor : public boost::static_visitor<void> {
public:
		OptionSerializeVisitor(NetworkSerializer& serializer) :
			m_serializer(serializer) {
		}

		template<typename TypeName>
		void operator()(const TypeName& param) const {
			LengthPlaceHolder<uint16_t, NetworkSerializer> lengthPlaceHolder( m_serializer, sizeof(param.m_type) );
			m_serializer.writeEnum(param.m_type);
			uint8_t reserved = 0;
			m_serializer << reserved;
			param.serialize(m_serializer);
		}
		NetworkSerializer& m_serializer;
	};

	void serialize(NetworkSerializer& serializer) const override {
		auto messageTypeAsUint8 = static_cast<uint8_t>(m_messageType);
		auto returnCodeAsUint8 = static_cast<uint8_t>(m_returnCode);
		auto flagsAsUint8 = *reinterpret_cast<const uint8_t*>(&m_flags);

		serializer << m_messageID;
		LengthPlaceHolder<uint32_t, NetworkSerializer> lengthPlaceHolder(serializer, 0);
		serializer << m_requestID << m_protocolVersion << m_interfaceVersion << messageTypeAsUint8 << returnCodeAsUint8;

		serializer << flagsAsUint8 << m_reserved1 << m_reserved2 << m_reserved3;

		{
			LengthPlaceHolder<uint32_t, NetworkSerializer> entryLengthPlaceHolder(serializer, 0);
			for (auto& entry : m_entries) {
				boost::apply_visitor(SerializeVisitor(serializer), entry);
			}
		}

		{
			LengthPlaceHolder<uint32_t, NetworkSerializer> optionLengthPlaceHolder(serializer, 0);
			for (auto& option : m_options) {
				boost::apply_visitor(OptionSerializeVisitor(serializer), option);
			}
		}
	}

	void assignHeader(SomeIP::SomeIPHeader& header) {
		SomeIPHeader::operator=(header);
	}

	void deserialize(NetworkDeserializer& serializer) {

		uint8_t flagsAsUint8;
		serializer >> flagsAsUint8 >> m_reserved1 >> m_reserved2 >> m_reserved3;
		m_flags = *reinterpret_cast<Flags*>(&flagsAsUint8);

		// read entries
		{
			uint32_t entriesLength;
			serializer >> entriesLength;

			size_t baseRemain = serializer.remainingBytesCount();
			while (baseRemain - serializer.remainingBytesCount() < entriesLength) {
				SomeIPServiceDiscoveryEntryHeader::Type type;
				serializer.readEnum(type);

				switch (type) {

				case SomeIPServiceDiscoveryEntryHeader::Type::FindService :
				case SomeIPServiceDiscoveryEntryHeader::Type::OfferService : {
					addEntry( SomeIPServiceDiscoveryServiceEntry(type, serializer) );
				}
				break;

				case SomeIPServiceDiscoveryEntryHeader::Type::Subscribe :
				case SomeIPServiceDiscoveryEntryHeader::Type::SubscribeAck : {
					addEntry( SomeIPServiceDiscoveryEventGroupEntry(type, serializer) );
				}
				break;

				default : {
					log_warning() << "Unknown entry type : " << static_cast<int>(type);
				}
				break;

				}
			}
		}

		// read options
		{
			uint32_t optionsLength;
			serializer >> optionsLength;

			size_t baseRemain = serializer.remainingBytesCount();
			while (baseRemain - serializer.remainingBytesCount() < optionsLength) {
				uint16_t length;
				serializer >> length;
				SomeIPServiceDiscoveryConfigurationOption::Type type;
				serializer.readEnum(type);
				uint8_t reserved;
				serializer >> reserved;

				switch (type) {

				case SomeIPServiceDiscoveryConfigurationOption::Type::IPv4Option : {
					addOption( IPv4ConfigurationOption(serializer) );
				}
				break;

				default : {
					log_warning() << "Unknown entry type : " << static_cast<int>(type);
				}
				break;

				}
			}
		}

	}

	const std::vector<ServiceEntryVariant>& getEntries() const {
		return m_entries;
	}

	const std::vector<ConfigurationOptionVariant>& getOptions() const {
		return m_options;
	}

	template<typename ServiceEntryType>
	void addEntry(const ServiceEntryType& entry) {
		m_entries.push_back(entry);
	}

	/**
	 * Return the index of the newly inserted element
	 */
	template<typename OptionType>
	size_t addOption(const OptionType& option) {
		m_options.push_back(option);
		return m_options.size() - 1;
	}

private:
	static uint16_t s_nextUnicastSessionID;
	static uint16_t s_nextMulticastSessionID;
	static bool s_UnicastReboot;
	static bool s_MulticastReboot;

	std::vector<ServiceEntryVariant> m_entries;
	std::vector<ConfigurationOptionVariant> m_options;

};

class SomeIPServiceDiscoveryServiceOfferedEntry : public SomeIPServiceDiscoveryServiceEntry {
public:
	SomeIPServiceDiscoveryServiceOfferedEntry(SomeIPServiceDiscoveryMessage& serviceDiscoveryMessage, ServiceIDs serviceID,
						  TransportProtocol protocol, IPV4Address address,
						  uint16_t ipv4Port) :
		m_ipv4ConfigurationOption(protocol, address, ipv4Port) {
		m_type = SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::OfferService;
		m_ttl = 0xFFFFFFFF;                     // This means the service does not expire
		m_serviceID = serviceID.serviceID;
		m_majorVersion = m_minorVersion = 1;
		m_instanceID = serviceID.instanceID;
		m_1 = 1;
		m_2 = 0;
		m_indexFirstOptionRun = m_indexSecondOptionRun = serviceDiscoveryMessage.addOption(m_ipv4ConfigurationOption);
	}

private:
	IPv4ConfigurationOption m_ipv4ConfigurationOption;

};

class SomeIPServiceDiscoveryServiceQueryEntry : public SomeIPServiceDiscoveryServiceEntry {
public:
	SomeIPServiceDiscoveryServiceQueryEntry(ServiceID serviceID) {
		m_type = SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::FindService;
		m_serviceID = serviceID; // means all instances should be returned
		m_majorVersion = 0xFF; // that means that services with any version shall be returned
		m_minorVersion = 0xFFFFFFFF; // that means that services with any version shall be returned
		m_instanceID = 0xFFFF; // Instance ID shall set to 0xFFFF, if all service instances shall be returned
		m_ttl = 0xFFFFFFFF;    // We don't want the service to expire
	}

};

class SomeIPServiceDiscoveryServiceUnregisteredEntry : public SomeIPServiceDiscoveryServiceEntry {
public:
	SomeIPServiceDiscoveryServiceUnregisteredEntry(SomeIPServiceDiscoveryMessage& serviceDiscoveryMessage,
						       ServiceIDs serviceID, TransportProtocol protocol, IPV4Address address,
						       uint16_t ipv4Port) : m_ipv4ConfigurationOption(protocol, address,
												      ipv4Port)
	{
		m_type = SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::OfferService;
		m_ttl = 0x0;   // The service expires immediately
		m_serviceID = serviceID.serviceID;
		m_majorVersion = m_minorVersion = 1;
		m_instanceID = serviceID.instanceID;
		m_1 = 0;
		m_2 = 0;
		m_indexFirstOptionRun = m_indexSecondOptionRun = serviceDiscoveryMessage.addOption(m_ipv4ConfigurationOption);;
	}

private:
	IPv4ConfigurationOption m_ipv4ConfigurationOption;

};

class SomeIPServiceDiscoverySubscribeNotificationEntry : public SomeIPServiceDiscoveryEventGroupEntry {
public:
	SomeIPServiceDiscoverySubscribeNotificationEntry(ServiceIDs serviceID, EventGroupID eventGroupID) {
		m_type = SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::Subscribe;
		m_ttl = 0xFFFFFFFF;   // No expiration
		m_serviceID = serviceID.serviceID;
		m_majorVersion = 1;
		m_instanceID = serviceID.instanceID;
		m_1 = 0;
		m_2 = 0;
		m_indexFirstOptionRun = m_indexSecondOptionRun = 0;
		m_eventGroupID = eventGroupID;
	}
};

class ServiceDiscoveryListener {
public:
	virtual ~ServiceDiscoveryListener() {
	}

	virtual void onRemoteClientSubscription(const SomeIPServiceDiscoveryEventGroupEntry& serviceEntry,
						const IPv4ConfigurationOption* address) = 0;

	virtual void onRemoteClientSubscriptionFinished(const SomeIPServiceDiscoveryEventGroupEntry& serviceEntry,
							const IPv4ConfigurationOption* address) = 0;

	virtual void onRemoteServiceAvailable(const SomeIPServiceDiscoveryServiceEntry& serviceEntry,
					      const IPv4ConfigurationOption* address,
					      const SomeIPServiceDiscoveryMessage& message) = 0;

	virtual void onRemoteServiceUnavailable(const SomeIPServiceDiscoveryServiceEntry& serviceEntry,
						const IPv4ConfigurationOption* address,
						const SomeIPServiceDiscoveryMessage& message) = 0;

	virtual void onFindServiceRequested(const SomeIPServiceDiscoveryServiceEntry& serviceEntry,
					    const SomeIPServiceDiscoveryMessage& message) = 0;

};

class ServiceDiscoveryMessageDecoder {

	LOG_DECLARE_CLASS_CONTEXT("SDMD", "ServiceDiscoveryMessageDecoder");

	ServiceDiscoveryMessageDecoder(const ServiceDiscoveryMessageDecoder& fdsf) = delete;

public:
	ServiceDiscoveryMessageDecoder(ServiceDiscoveryListener& listener) :
		m_listener(listener) {
	}

	class IPv4AddressExtractor : public boost::static_visitor<const IPv4ConfigurationOption*> {
public:
		const IPv4ConfigurationOption* operator()(const IPv4ConfigurationOption& configurationOption)
		const {
			return &configurationOption;
		}

		template<typename T>
		const IPv4ConfigurationOption* operator()(const T& v) const {
			return nullptr;
		}
	};

	class EntryVisitor : public boost::static_visitor<void> {
public:
		EntryVisitor(ServiceDiscoveryMessageDecoder& serviceListener, const SomeIPServiceDiscoveryMessage& message) :
			m_serviceListener(serviceListener), m_message(message) {
		}

		void operator()(const SomeIPServiceDiscoveryServiceEntry& entry) const {
			switch (entry.m_type) {
			case SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::OfferService : {
				IPv4AddressExtractor ext;
				auto addressOption = boost::apply_visitor(ext,
									  m_message.getOptions()[entry.m_indexFirstOptionRun]);
				assert(addressOption != nullptr);
				if (entry.m_ttl != 0) {
					m_serviceListener.m_listener.onRemoteServiceAvailable(entry, addressOption, m_message);
				} else {
					m_serviceListener.m_listener.onRemoteServiceUnavailable(entry, addressOption, m_message);
				}
			}
			break;

			case SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::FindService : {
				log_info() << "Query";
				m_serviceListener.m_listener.onFindServiceRequested(entry, m_message);
			}
			break;

			default :
				assert(false);
				break;
			}
		}

		void operator()(const SomeIPServiceDiscoveryEventGroupEntry& entry) const {
			switch (entry.m_type) {
			case SomeIP::SomeIPServiceDiscoveryEntryHeader::Type::Subscribe : {
				IPv4AddressExtractor ext;

				const IPv4ConfigurationOption* addressOption = nullptr;

				if (entry.m_1 != 0)
					addressOption =
						boost::apply_visitor(ext,
								     m_message.getOptions()[entry.m_indexFirstOptionRun]);

				//				assert(addressOption != nullptr);
				if (entry.m_ttl != 0) {
					m_serviceListener.m_listener.onRemoteClientSubscription(entry, addressOption);
				} else {
					m_serviceListener.m_listener.onRemoteClientSubscriptionFinished(entry, addressOption);
				}

			}
			break;

			default :
				assert(false);
				break;

			}

		}

		ServiceDiscoveryMessageDecoder& m_serviceListener;
		const SomeIPServiceDiscoveryMessage& m_message;
	};

	void decodeMessage(const SomeIP::SomeIPHeader& header, const void* payload, size_t payloadLength) {
		NetworkDeserializer deserializer(payload, payloadLength);

		SomeIPServiceDiscoveryMessage message(header, deserializer);
		EntryVisitor visitor(*this, message);
		for ( auto& entry : message.getEntries() ) {
			boost::apply_visitor(visitor, entry);
		}

	}

	void decodeMessage(const void* payload, size_t payloadLength) {
		NetworkDeserializer deserializer(payload, payloadLength);
		SomeIPServiceDiscoveryMessage message(deserializer);

		EntryVisitor visitor(*this, message);
		for ( auto& entry : message.getEntries() ) {
			boost::apply_visitor(visitor, entry);
		}
	}

private:
	ServiceDiscoveryListener& m_listener;

};
}
