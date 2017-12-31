/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  pypilot Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2017 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */

#include <vector>

#include <wx/wx.h>
#include <wx/socket.h>

#include "signalk_client.h"

BEGIN_EVENT_TABLE(SignalKClient, wxEvtHandler)
    EVT_SOCKET(-1, SignalKClient::OnSocketEvent)
END_EVENT_TABLE()

SignalKClient::SignalKClient()
{
//    m_sock.Connect(wxEVT_SOCKET, wxSocketEventHandler(SignalKClient::OnSocketEvent), NULL, this);
    
    m_sock.SetEventHandler(*this);
    m_sock.SetNotify(wxSOCKET_CONNECTION_FLAG | wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    m_sock.Notify(true);
    m_sock.SetTimeout(1);
}

void SignalKClient::connect(wxString host, int port)
{
    if(host.empty())
        host = "pypilot";
            
    if(port == 0)
        port = 21311; /* default port */

    wxIPV4address addr;
    addr.Hostname(host);
    addr.Service(port);
    m_sock.Connect(addr, false);
}

bool SignalKClient::receive(wxJSONValue &value)
{
    if(m_values.empty())
        return false;

    value = m_values.front();
    m_values.pop_front();
    return true;
}

void SignalKClient::get(wxString name)
{
    wxJSONValue request;
    request["method"] = "get";
    request["name"] = name;
    send(request);
}

void SignalKClient::set(wxString name, wxJSONValue &value)
{
    wxJSONValue request;
    request["method"] = "get";
    request["name"] = name;
    request["value"] = value;
    send(request);
}

void SignalKClient::watch(wxString name, bool on)
{
    wxJSONValue request;
    request["method"] = "watch";
    request["name"] = name;
    request["value"] = on;
    send(request);
}

void SignalKClient::request_list_values()
{
    m_values.clear();

    wxJSONValue request;
    request["method"] = "list";
    send(request);
//    m_bRequestList = true;
}

void SignalKClient::send(wxJSONValue &request)
{
    wxJSONWriter writer;
    wxString str;
    writer.Write(request, str);
    str += "\n";
    m_sock.Write(str.mb_str(), str.Length());
}

void SignalKClient::OnSocketEvent(wxSocketEvent& event)
{
    switch(event.GetSocketEvent())
    {
        case wxSOCKET_CONNECTION:
            OnConnected();
            break;

        case wxSOCKET_LOST:
            OnDisconnected();
            break;

        case wxSOCKET_INPUT:
    #define RD_BUF_SIZE    4096
            std::vector<char> data(RD_BUF_SIZE+1);
            event.GetSocket()->Read(&data.front(),RD_BUF_SIZE);
            if(!event.GetSocket()->Error())
            {
                size_t count = event.GetSocket()->LastCount();
                if(count)
                {
                    data[count]=0;
//                    m_sock_buffer.Append(data);
                    m_sock_buffer += (&data.front());
                }
            }

            size_t line_end = m_sock_buffer.find_first_of("*\r\n");
            std::string json_line = m_sock_buffer.substr(0, line_end);
            wxJSONValue value;
            wxJSONReader reader;
            if(reader.Parse(json_line, &value)) {
                const wxArrayString& errors = reader.GetErrors();
                wxString sLogMessage;
                sLogMessage.Append(wxT("pypilot_pi: Error parsing JSON message - "));
                sLogMessage.Append( json_line );
                for(size_t i = 0; i < errors.GetCount(); i++)
                {
                    sLogMessage.append( errors.Item( i ) );
                }
                wxLogMessage( sLogMessage );
            } else {
                m_values.push_back(value);
            }
            if(line_end > m_sock_buffer.size())
                m_sock_buffer.clear();
            else
                m_sock_buffer = m_sock_buffer.substr(line_end);
            break;
    }
}
