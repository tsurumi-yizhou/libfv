#ifndef __FV_CONN_IMPL_HPP__
#define __FV_CONN_IMPL_HPP__



#include <regex>
#include <unordered_set>

#include "common.hpp"
#include "base.hpp"
#include "conn.hpp"



namespace fv {
inline Task<char> IConn2::ReadChar () {
	if (TmpData.size () == 0) {
		char _buf [1024];
		size_t _n = co_await RecvImpl (_buf, sizeof (_buf));
		TmpData.resize (_n);
		::memcpy (&TmpData [0], _buf, _n);
	}
	char _ret = TmpData [0];
	TmpData.erase (0);
	co_return _ret;
}

inline Task<std::string> IConn2::ReadLine () {
	std::string _tmp = "";
	size_t _p;
	while ((_p = TmpData.find ('\n')) == std::string::npos) {
		char _buf [1024];
		size_t _readed = co_await RecvImpl (_buf, sizeof (_buf));
		TmpData += std::string_view { _buf, _readed };
	}
	_tmp = TmpData.substr (0, _p);
	if (_tmp [_p - 1] == '\r')
		_tmp.erase (_p - 1);
	TmpData.erase (0, _p + 1);
	co_return _tmp;
}

inline Task<std::string> IConn2::ReadCount (size_t _count) {
	if (_count == 0)
		co_return "";
	std::string _tmp = "";
	while (TmpData.size () < _count) {
		char _buf [1024];
		size_t _readed = co_await RecvImpl (_buf, sizeof (_buf));
		TmpData += std::string_view { _buf, _readed };
	}
	_tmp = TmpData.substr (0, _count);
	TmpData.erase (0, _count);
	co_return _tmp;
}

inline Task<std::vector<uint8_t>> IConn2::ReadCountVec (size_t _count) {
	std::string _tmp = co_await ReadCount (_count);
	std::vector<uint8_t> _vec;
	_vec.assign ((uint8_t *) &_tmp [0], (uint8_t *) &_tmp [_count]);
	co_return _vec;
}

inline Task<std::string> IConn2::ReadSome () {
	if (TmpData.size () > 0) {
		std::string _ret = std::move (TmpData);
		TmpData = "";
		co_return _ret;
	} else {
		char _buf [1024];
		size_t _len = co_await RecvImpl (_buf, sizeof (_buf));
		co_return std::string (_buf, _len);
	}
}



inline Task<void> TcpConn::Connect (std::string _host, std::string _port) {
	m_host = _host;
	m_port = _port;
	co_await Reconnect ();
}

inline Task<void> TcpConn::Reconnect () {
	Close ();
	TmpData = "";
	std::regex _r { "(\\d+\\.){3}\\d+" };
	std::string _ip = m_host;
	if (!std::regex_match (m_host, _r)) {
		auto _v = co_await Config::DnsResolve (m_host);
		if (_v.size () == 0)
			throw Exception (fmt::format ("Cannot resolve host {}", m_host));
		_ip = _v [0];
	}
	uint16_t _sport = (uint16_t) std::stoi (m_port);
	co_await Socket.async_connect (Tcp::endpoint { asio::ip::address::from_string (_ip), _sport }, UseAwaitable);
	if (!Socket.is_open ())
		throw Exception (fmt::format ("Cannot connect to server {}", m_host));
	if (Config::NoDelay)
		Socket.set_option (Tcp::no_delay { true });
}

inline void TcpConn::Close () {
	if (Socket.is_open ()) {
		//Socket.shutdown (SocketBase::shutdown_both);
		Socket.close ();
	}
}

inline Task<void> TcpConn::Send (char *_data, size_t _size) {
	if (!Socket.is_open ())
		throw Exception ("Cannot send data to a closed connection.");
	size_t _sended = 0;
	while (_sended < _size) {
		size_t _tmp_send = co_await Socket.async_send (asio::buffer (&_data [_sended], _size - _sended), UseAwaitable);
		if (_tmp_send == 0)
			throw Exception ("Connection temp closed.");
		_sended += _tmp_send;
	}
}

inline Task<size_t> TcpConn::RecvImpl (char *_data, size_t _size) {
	if (!Socket.is_open ())
		co_return 0;
	size_t _count = co_await Socket.async_read_some (asio::buffer (_data, _size), UseAwaitable);
	co_return _count;
}

inline void TcpConn::Cancel () {
	Socket.cancel ();
}



inline TcpConn2::TcpConn2 (Tcp::socket _sock): Socket (std::move (_sock)) {
	if (Config::NoDelay)
		Socket.set_option (Tcp::no_delay { true });
}

inline void TcpConn2::Close () {
	if (Socket.is_open ()) {
		//Socket.shutdown (SocketBase::shutdown_both);
		Socket.close ();
	}
}

inline Task<void> TcpConn2::Send (char *_data, size_t _size) {
	if (!Socket.is_open ())
		throw Exception ("Cannot send data to a closed connection.");
	size_t _sended = 0;
	while (_sended < _size) {
		size_t _tmp_send = co_await Socket.async_send (asio::buffer (&_data [_sended], _size - _sended), UseAwaitable);
		if (_tmp_send == 0)
			throw Exception ("Connection temp closed.");
		_sended += _tmp_send;
	}
}

inline Task<size_t> TcpConn2::RecvImpl (char *_data, size_t _size) {
	if (!Socket.is_open ())
		co_return 0;
	size_t _count = co_await Socket.async_read_some (asio::buffer (_data, _size), UseAwaitable);
	co_return _count;
}



inline Task<void> SslConn::Connect (std::string _host, std::string _port) {
	m_host = _host;
	m_port = _port;
	co_await Reconnect ();
}

inline Task<void> SslConn::Reconnect () {
	Close ();
	TmpData = "";
	SslSocket.set_verify_mode (Ssl::verify_peer);
	SslSocket.set_verify_callback (Config::SslVerifyFunc);
	std::regex _r { "(\\d+\\.){3}\\d+" };
	std::string _ip = m_host;
	if (!std::regex_match (m_host, _r)) {
		auto _v = co_await Config::DnsResolve (m_host);
		if (_v.size () == 0)
			throw Exception (fmt::format ("Cannot resolve host {}", m_host));
		_ip = _v [0];
	}
	uint16_t _sport = (uint16_t) std::stoi (m_port);
	co_await SslSocket.next_layer ().async_connect (Tcp::endpoint { asio::ip::address::from_string (_ip), _sport }, UseAwaitable);
	if (!SslSocket.next_layer ().is_open ())
		throw Exception (fmt::format ("Cannot connect to server {}", m_host));
	if (Config::NoDelay)
		SslSocket.next_layer ().set_option (Tcp::no_delay { true });
	co_await SslSocket.async_handshake (Ssl::stream_base::client, UseAwaitable);
}

inline void SslConn::Close () {
	if (SslSocket.next_layer ().is_open ()) {
		//SslSocket.next_layer ().shutdown (SocketBase::shutdown_both);
		SslSocket.next_layer ().close ();
	}
}

inline Task<void> SslConn::Send (char *_data, size_t _size) {
	if (!SslSocket.next_layer ().is_open ())
		throw Exception ("Cannot send data to a closed connection.");
	size_t _sended = 0;
	while (_sended < _size) {
		size_t _tmp_send = co_await SslSocket.async_write_some (asio::buffer (&_data [_sended], _size - _sended), UseAwaitable);
		if (_tmp_send == 0)
			throw Exception ("Connection temp closed.");
		_sended += _tmp_send;
	}
}

inline Task<size_t> SslConn::RecvImpl (char *_data, size_t _size) {
	co_return co_await SslSocket.async_read_some (asio::buffer (_data, _size), UseAwaitable);
}

inline void SslConn::Cancel () {
	SslSocket.next_layer ().cancel ();
}



inline SslConn2::SslConn2 (Ssl::stream<Tcp::socket> _sock): SslSocket (std::move (_sock)) {
	if (Config::NoDelay)
		SslSocket.next_layer ().set_option (Tcp::no_delay { true });
}

inline void SslConn2::Close () {
	if (SslSocket.next_layer ().is_open ()) {
		//SslSocket.next_layer ().shutdown (SocketBase::shutdown_both);
		SslSocket.next_layer ().close ();
	}
}

inline Task<void> SslConn2::Send (char *_data, size_t _size) {
	if (!SslSocket.next_layer ().is_open ())
		throw Exception ("Cannot send data to a closed connection.");
	size_t _sended = 0;
	while (_sended < _size) {
		size_t _tmp_send = co_await SslSocket.async_write_some (asio::buffer (&_data [_sended], _size - _sended), UseAwaitable);
		if (_tmp_send == 0)
			throw Exception ("Connection temp closed.");
		_sended += _tmp_send;
	}
}

inline Task<size_t> SslConn2::RecvImpl (char *_data, size_t _size) {
	if (!SslSocket.next_layer ().is_open ())
		co_return 0;
	size_t _count = co_await SslSocket.async_read_some (asio::buffer (_data, _size), UseAwaitable);
	co_return _count;
}



inline WsConn::WsConn (std::shared_ptr<IConn2> _parent, bool _is_client): Parent (_parent), IsClient (_is_client) {
	fv::Tasks::RunAsync ([_wptr = std::weak_ptr (shared_from_this ())] () -> Task<void> {
		while (true) {
			co_await Tasks::Delay (Config::WebsocketAutoPing);
			auto _ptr = _wptr.lock ();
			if (!_ptr)
				co_return;
			_ptr->_Send (nullptr, 0, WsType::Ping);
		}
	});
}



inline Task<std::tuple<std::string, WsType>> WsConn::Recv () {
	std::vector<uint8_t> _tmp = co_await Parent->ReadCountVec (2);
	bool _is_eof = (_tmp [0] & 0x80) != 0;
	WsType _type = (WsType) (_tmp [0] & 0xf);
	if (_type == WsType::Close) {
		Close ();
		throw Exception ("Remote send close msg.");
	} else if (_type == WsType::Ping) {
		co_await _Send (nullptr, 0, WsType::Pong);
		co_return co_await Recv ();
	} else if (_type == WsType::Pong) {
		//co_return std::make_tuple (std::string (""), WsType::Pong);
		co_return co_await Recv ();
	} else if (_type == WsType::Text || _type == WsType::Binary) {
		bool _mask = _tmp [1] & 0x80;
		long _payload_length = (long) (_tmp [1] & 0x7f);
		if (_payload_length == 126) {
			_tmp = co_await Parent->ReadCountVec (2);
			_payload_length = (((long) _tmp [0]) << 8) + _tmp [1];
		} else if (_payload_length == 127) {
			_tmp = co_await Parent->ReadCountVec (8);
			_payload_length = 0;
			for (int i = 0; i < 8; ++i)
				_payload_length = (_payload_length << 8) + _tmp [i];
		}
		if (_mask)
			_tmp = co_await Parent->ReadCountVec (4);
		std::string _s = co_await Parent->ReadCount ((size_t) _payload_length);
		if (_mask) {
			for (size_t i = 0; i < _s.size (); ++i)
				_s [i] ^= _tmp [i % 4];
		}
		if (_type == WsType::Continue) {
			auto [_s1, _type1] = co_await Recv ();
			_s += _s1;
			co_return std::make_tuple (_s, _type1);
		} else {
			co_return std::make_tuple (_s, _type);
		}
	} else {
		Parent = nullptr;
		throw Exception ("Unparsed websocket frame.");
	}
	co_return std::make_tuple (std::string (""), WsType::Close);
}

inline Task<void> WsConn::_Send (char *_data, size_t _size, WsType _type) {
	if (!IsConnect ()) {
		if (_type == WsType::Close)
			co_return;
		throw Exception ("Cannot send data to a closed connection.");
	}
	std::stringstream _ss;
	_ss << (char) (0x80 | (char) _type);
	static std::vector<char> _mask = { (char) 0xfa, (char) 0xfb, (char) 0xfc, (char) 0xfd };
	if (_size > 0) {
		if (_size < 126) {
			_ss << (char) (_size);
		} else if (_size < 0xffff) {
			_ss << (char) (126) << (char) ((_size >> 8) & 0xff) << (char) (_size & 0xff);
		} else {
			_ss << (char) (127);
			int64_t _size64 = (int64_t) _size;
			for (int i = 7; i >= 0; --i)
				_ss << (char) ((_size64 >> (i * 8)) & 0xff);
		}
		if (IsClient) {
			for (size_t i = 0; i < 4; ++i)
				_ss << _mask [i];
		}
		_ss << std::string_view { _data, _size };
	} else {
		_ss << '\0';
	}
	std::string _to_send = _ss.str ();
	if (IsClient) {
		for (size_t i = 6; i < _to_send.size (); ++i)
			_to_send [i] ^= _mask [i % 4];
	}
	if (_type == WsType::Close) {
		try {
			co_await Parent->Send (_to_send.data (), _to_send.size ());
		} catch (...) {
		}
	} else {
		co_await Parent->Send (_to_send.data (), _to_send.size ());
	}
}



inline Task<std::shared_ptr<IConn>> Connect (std::string _url) {
	auto [_schema, _host, _port, _path] = _parse_url (_url);
	//
	if (_schema == "tcp") {
		if (_path != "/")
			throw Exception ("Url format error");
		auto _conn = std::shared_ptr<IConn> (new TcpConn {});
		co_await _conn->Connect (_host, _port);
		co_return _conn;
	} else {
		throw Exception (fmt::format ("Unknown protocol: {}", _schema));
	}
}

inline Task<std::shared_ptr<WsConn>> ConnectWS (std::string _url) {
	auto [_schema, _host, _port, _path] = _parse_url (_url);
	if (_schema == "ws" || _schema == "wss") {
		// connect
		auto _conn = std::shared_ptr<IConn> (_schema == "ws" ? (IConn *) new TcpConn {} : new SslConn {});
		co_await _conn->Connect (_host, _port);

		// generate data
		Request _r { _url, MethodType::Get };
		_r.Headers ["Pragma"] = "no-cache";
		_r.Headers ["Connection"] = "Upgrade";
		_r.Headers ["Upgrade"] = "websocket";
		_r.Headers ["Sec-WebSocket-Version"] = "13";
		_r.Headers ["Sec-WebSocket-Key"] = "libfvlibfv==";
		std::string _data = _r.Serilize (_host, _path);

		// send
		co_await _conn->Send (_data.data (), _data.size ());

		// recv
		co_await Response::GetFromConn (_conn);
		co_return std::make_shared<WsConn> (_conn, true);
	} else {
		throw Exception (fmt::format ("Unknown protocol: {}", _schema));
	}
}
}



#endif //__FV_CONN_IMPL_HPP__
