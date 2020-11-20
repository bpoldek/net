#include "net.h"

namespace net::aoe::stack
{

Server::Server(int major, int minor, int sectors, Handler_transmitt transmitt, Handler_read read, Handler_write write, Handler_print print)
: _major(major), _minor(minor), _sectors(sectors), _transmitt(transmitt), _read(read), _write(write), _print(print)
{
    _clear();
}


void Server::init()
{
    _debug("Initializing AOE Server ...");

    _fill_header_eth(eth::address::Broadcast);
    _fill_header_aoe(aoe::command::Query_config_information, 0);
    _fill_data_aoe_query();

    _clear();

    _debug("Done, waiting for AOE transmission ...");
}

void Server::process(unsigned char * data)
{
    if (auto valid = _check_valid(data); valid == false) return;

    _fill_header_eth(eth::Header(data).source);
    _fill_header_aoe(aoe::Header(data).command, aoe::Header(data).tag);

    auto result_transmitt = false;

    if (aoe::Header(data).command == aoe::command::Query_config_information)
    {
        _debug("RX: Query config information");

        _fill_data_aoe_query();

        result_transmitt = _transmitt(_buffer, 32, timeout);
    }
    else if (aoe::Header(data).command == aoe::command::Issue_ata_command)
    {
        if (aoe::issue::Header(data).command == aoe::issue::command::Identify_Device)
        {
            _debug("RX: Issue identify device");

            _fill_data_aoe_issue_identify();
            _fill_header_aoe_issue(aoe::issue::Header(data).lba, true);

            result_transmitt = _transmitt(_buffer, 512 + 36, timeout);
        }
        else if (aoe::issue::Header(data).command == aoe::issue::command::Read_sectors_with_retry)
        {
            unsigned char sector_count = (unsigned char) aoe::issue::Header(data).sector_count;
            unsigned int lba = (unsigned int) aoe::issue::Header(data).lba & 0x00ffffff;

            _debug("RX: [R], N = [%d], LBA = [%d]", sector_count, lba);

            auto result_read = _fill_data_aoe_issue_read(sector_count, lba);
            _fill_header_aoe_issue(aoe::issue::Header(data).lba, result_read);

            if (result_read == false) _debug("Read error ...");
            else result_transmitt = _transmitt(_buffer, 36 + 512 * sector_count, timeout);
        }
        else if (aoe::issue::Header(data).command == aoe::issue::command::Write_sectors_with_retry)
        {
            unsigned char sector_count = (unsigned char) aoe::issue::Header(data).sector_count;
            unsigned int lba = (unsigned int) aoe::issue::Header(data).lba & 0x00ffffff;

            _debug("RX: [W], N = [%d], LBA = [%d]", sector_count, lba);

            auto result_write = _fill_data_aoe_issue_write(data, sector_count, lba);
            _fill_header_aoe_issue(aoe::issue::Header(data).lba, result_write);

            if (result_write == false) _debug("Write error ...");
            else result_transmitt = _transmitt(_buffer, 36, timeout);
        }
        else // command unknown
        {
            _debug("Invalid ATA command received ...");
        }       
    }
    else // command unknown
    {
        _debug("Invalid AOE command received ...");
    }
    
    if (result_transmitt == false) _debug("Transmitt error, unable to send data ...");

    _clear();
}

/* ---------------------------------------------| info |--------------------------------------------- */

void Server::_clear()
{
    memset(_buffer, 0, size_buffer);
}

bool Server::_check_valid(unsigned char * data)
{
    auto header_eth = eth::Header(data);

    if (header_eth.destination != eth::address::Broadcast && header_eth.destination != eth::address::Empty) return false;
    if (header_eth.type != eth::type::Aoe) return false;

    auto header_aoe = aoe::Header(data);

    if (header_aoe.version != aoe_version) return false;
    if (header_aoe.flag_error == true) return false;
    if (header_aoe.flag_response == true) return false;
    if (header_aoe.error != (unsigned char) 0) return false;
    if (header_aoe.address_major != _major && header_aoe.address_major != (unsigned short int) 0xffff) return false;
    if (header_aoe.address_minor != _minor && header_aoe.address_minor != (unsigned char) 0xff) return false;

    return true;
}

void Server::_fill_header_eth(const eth::address::Custom & destination)
{
    auto header_eth = eth::Header(_buffer);

    header_eth.destination = destination;
    header_eth.source = eth::address::Empty;
    header_eth.type = eth::type::Aoe;
}

void Server::_fill_header_aoe(unsigned char command, unsigned int tag)
{
    auto header_aoe = aoe::Header(_buffer);

    header_aoe.version = aoe_version;
    header_aoe.flag_response = true;
    header_aoe.flag_error = false;
    header_aoe.error = false;
    header_aoe.address_major = _major;
    header_aoe.address_minor = _minor;
    header_aoe.command = command;
    header_aoe.tag = tag;
}

void Server::_fill_header_aoe_issue(unsigned int lba, bool ready)
{
    auto header_issue = aoe::issue::Header(_buffer);

    if (ready == true) header_issue.status_device_ready = true;
    else header_issue.status_error = true;

    header_issue.lba = lba;
}

void Server::_fill_data_aoe_query()
{
    auto header_query = aoe::query::Header(_buffer);

    header_query.buffer_count = buffer_count;
    header_query.firmware_version = firmware_version;
    header_query.sector_count = sector_count;
    header_query.aoe_version = aoe_version;
    header_query.command = aoe::query::command::Read_config_string;
    header_query.config_string_length = 0;
}

void Server::_fill_data_aoe_issue_identify()
{
    auto header_identify = aoe::issue::identify::Header(_buffer);

    header_identify.logical_sector_number = _sectors;
}

bool Server::_fill_data_aoe_issue_read(unsigned char sector_count, unsigned int lba)
{
    for (int i = 0; i < sector_count; i++) 
    {
        if (auto result = _read(&_buffer[i * 512 + 36], lba + i); result == false) return false;  
    }

    return true;
}

bool Server::_fill_data_aoe_issue_write(unsigned char * data, unsigned char sector_count, unsigned int lba)
{
    for (int i = 0; i < sector_count; i++) 
    {
        if (auto result = _write(&data[i * 512 + 36], lba + i); result == false) return false;
    }

    return true;
}

}; /* namespace: net::aoe::stack */
