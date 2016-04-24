/*
 * ZSUMMER License
 * -----------
 * 
 * ZSUMMER is licensed under the terms of the MIT license reproduced below.
 * This means that ZSUMMER is free software and can be used for both academic
 * and commercial purposes at absolutely no cost.
 * 
 * 
 * ===============================================================================
 * 
 * Copyright (C) 2010-2013 YaweiZhang <yawei_zhang@foxmail.com>.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * ===============================================================================
 * 
 * (end of COPYRIGHT)
 */
#ifndef _ZSUMMER_TCPACCEPT_H_
#define _ZSUMMER_TCPACCEPT_H_

#include "public.h"
namespace zsummer
{
	using namespace zsummer::network;

	class CTcpAccept : public ITcpAccept
	{
	public:
		CTcpAccept();
		virtual ~CTcpAccept();
		virtual bool Initialize(IIOServer * ios, ITcpAcceptCallback * cb);
		virtual bool OpenAccept(const char * ip, unsigned short port);


		virtual bool OnIOCPMessage(BOOL bSuccess);
		virtual bool Close();
		void OnClear();

		//config
		IIOServer			*m_ios;
		ITcpAcceptCallback  *m_cb;
		std::string		m_ip;
		short			m_port;
		//listen
		SOCKET			m_server;
		SOCKADDR_IN		m_addr;

		//client
		SOCKET m_socket;
		char m_recvBuf[200];
		DWORD m_recvLen;
		tagReqHandle m_handle;

		//status
		int m_nAcceptCount;
		int m_nLinkStatus;
	};

}





















#endif











