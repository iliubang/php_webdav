# php_webdav扩展

一个很简单的php_webdav扩展，仅仅用来研究webdav协议的原理

支持PUT, GET, DELETE, POST请求



#安装


```bash
cd php_webdav
phpize
./configure --with-php-config={your_php_bin_path}/php-config
make && sudo make install
echo 'extension=webdav.so' >> {your_php_ini_path}/php.ini

```

# 使用

```php
<?php
$webdav = new Webdav("webdav.iliubang.cn");
var_dump($webdav);
//本地文件，目标路径
$res = $webdav->upload("1.jpg", "/test/1.jpg",);
var_dump($res);
//远程文件，存放的本地路径
$res = $webdav->get("/test/4.jpg", "/home/liubang/workspace/c/php_webdav/4.jpg");
var_dump($res);
//远程文件
$res = $webdav->delete('/test/1.jpg');
var_dump($res);

$webdav = new Webdav("linger.iliubang.cn");

$arr = array('name' => 'liubang', 'age' => 23, 'gender' => 'nan');
// $res = $webdav->post('/', "name=liubang&age=sadgad");
$res = $webdav->post('/', $arr);

echo $res;


==================
liubang@shop-dev:~ $ php test.php
object(Webdav)#1 (1) {
  ["_host":protected]=>
  string(18) "webdav.iliubang.cn"
}
bool(true)
bool(true)
bool(true)
{"name":"liubang","age":"23","gender":"nan"}
```