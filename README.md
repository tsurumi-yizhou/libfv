# libfv

English | [��������](./README.zh.md)

A fast C++ async http library based on boost.asio.

Example:

```cpp
#include <fv/fv.hpp>

// ...

fv::Response _r = co_await fv::Get ("http://www.fawdlstty.com");
std::cout << _r.Content;
```

Roadmap:

- [x] HttpGet
- [ ] HttpPost
- [ ] TCP
- [ ] UDP
- [ ] Websocket
- [ ] SSL
