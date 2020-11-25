#ifndef _net_aoe_stack_client_h
#define _net_aoe_stack_client_h

/**
 * @file	net_aoe_stack_client.h
 * @author	en2
 * @date	04-11-2020
 * @brief	
 * @details	
**/

#include "net_aoe_stack_actor.h"

namespace net::aoe::stack
{

class Client : public Actor
{
public:
    Client(net::interface::Io & interface_io);

    unsigned int connect();

    bool read(unsigned char * to, unsigned int sector, int count = 1);
    bool write(unsigned char * from, unsigned int sector, int count = 1);

protected:
    int _get_query_response();
    int _get_identify_response();

    eth::address::Custom _destination;
    unsigned int _sectors;
    unsigned int _tag = 0;

private:

}; /* class: Client */


}; /* namespace: net::aoe::stack */


#endif /* define: net_aoe_stack_client_h */