# 样本格式

deepx支持以下样本格式.

- [libsvm](https://www.csie.ntu.edu.tw/~cjlin/libsvm)
- libsvm\_ex
- uch(user candidate history)

## libsvm

deepx将标准libsvm格式进行了扩展, 扩展后的libsvm格式样本由以下几部分组成.

```
标签系列 uuid部分(可选) 特征系列
```

1条样本包含1个标签系列, 可选的uuid部分, 1个特征系列.

### 标签系列

标签系列包含1~32个"标签"或"<标签, 权重>对".

"标签"单独出现时, "权重"是1.

"标签"和"权重"用冒号(:)分隔.

"标签"或"<标签, 权重>对"之间用空格分隔.

#### 标签

标签是浮点数.

为防止脏数据, 标签的有效范围是[-10000, 10000].

#### 权重

权重是浮点数.

为防止脏数据, 权重的有效范围是(0, 10000].

#### 例子

标签(权重)是1(1).

```
1
```

标签(权重)是1(0.5).

```
1:0.5
```

标签(权重)分别是1(1), -1(1).

```
1 -1
```

标签(权重)分别是1(0.5), -1(1).

```
1:0.5 -1
```

标签(权重)分别是1(0.5), -1(0.5).

```
1:0.5 -1:0.5
```

### uuid部分

uuid部分由"uuid:"开头, 后面跟随"uuid字符串".

"uuid字符串"不能包含空格, 制表符(\t), 竖线(|)等特殊字符.

"uuid字符串"建议由数字和字母组成.

#### 例子

uuid是"10000".

```
uuid:10000
```

uuid是"abcde".

```
uuid:abcde
```

### 特征系列

特征系列包含0到若干个"特征id"或"<特征id, 特征值>对".

"特征id"单独出现时, "特征值"是1.

"特征id"和"特征值"用冒号(:)分隔.

"特征id"或"<特征id, 特征值>对"之间用空格分隔.

#### 特征id

参考[特征](feature.md).

#### 特征值

参考[特征](feature.md).

#### 例子

特征系列不包含特征.

```

```

特征id(特征值)是10000(1).

```
10000
```

特征id(特征值)是10000(0.5).

```
10000:0.5
```

特征id(特征值)分别是10000(1), 20000(1), 30000(1).

```
10000 20000 30000
```

特征id(特征值)分别是10000(0.75), 20000(0.5), 30000(0.25).

```
10000:0.75 20000:0.5 30000:0.25
```

### 例子

标签(权重)是1(1).

特征id(特征值)分别是10000(1), 20000(1), 30000(1).

```
1 10000 20000 30000
```

标签(权重)是-1(1).

uuid是"abcde".

特征id(特征值)分别是10000(0.75), 20000(0.5), 30000(0.25).

```
-1 uuid:abcde 10000:0.75 20000:0.5 30000:0.25
```

## libsvm\_ex

libsvm\_ex格式样本由以下几部分组成.

```
标签系列 uuid部分(可选) 特征系列1|特征系列2...
```

1条样本包含1个标签系列, 可选的uuid部分, 1~128个特征系列.

标签系列, uuid部分, 特征系列的格式参考libsvm.

### 例子

标签(权重)是1(1).

特征系列1的特征id(特征值)分别是10000(1), 20000(1), 30000(1).

特征系列2的特征id(特征值)分别是40000(1), 50000(1).

```
1 10000 20000 30000|40000 50000
```

标签(权重)分别是1(1), -1(1).

uuid是"abcde".

特征系列1的特征id(特征值)分别是10000(0.75), 20000(0.5), 30000(0.25).

特征系列2的特征id(特征值)分别是40000(0.75), 50000(0.5).

```
1 -1 uuid:abcde 10000:0.75 20000:0.5 30000:0.25|40000:0.75 50000:0.5
```

## uch

uch格式样本由以下几部分组成.

```
标签系列 uuid部分(可选) user特征系列|candidate特征系列|history特征系列1|history特征系列2...
```

1条样本包含1个标签系列, 可选的uuid部分, 1个user特征系列, 1个candidate特征系列, 0~128个history特征系列.

标签系列, uuid部分, 特征系列的格式参考libsvm.

### 例子

标签(权重)是1(1).

uuid是"abcde".

user特征系列的特征id(特征值)分别是10000(0.75), 20000(0.5), 30000(0.25).

candidate特征系列的特征id(特征值)分别是40000(0.75), 50000(0.5).

history特征系列1的特征id(特征值)分别是60000(1), 70000(1).

history特征系列2的特征id(特征值)分别是80000(1), 90000(1).

...

```
1 uuid:abcde 10000:0.75 20000:0.5 30000:0.25|40000:0.75 50000:0.5|60000 70000|80000 90000|...
```