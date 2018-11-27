//
//  http.h
//  xdag
//
//  Created by Rui Xie on 5/25/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef http_h
#define http_h

#ifdef __cplusplus
extern "C" {
#endif
	
// 简单的HTTP获取、传递URL，并用Malc返回内容。需要释放返回值。
extern char *http_get(const char* url);

extern int test_https(void);
extern int test_http(void);
	
#ifdef __cplusplus
};
#endif

#endif /* http_h */
