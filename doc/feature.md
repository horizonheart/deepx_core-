# 特征

## 特征id

deepx中, 特征id是64位无符号整数.

### 特征组

如果需要支持特征组, 特征id将这样编码.

```
高16位   | 低48位
特征组id | 子特征id
```

特征组id占据高16位, 它的有效范围是[0, 65535], 建议跳过0, 从1开始.

子特征id占据低48位.

### 特征组例子

原始数据是.

```
性别:男 年龄:20 爱好:篮球 爱好:乒乓球
性别:女 年龄:30 爱好:乒乓球
性别:男 年龄:25 爱好:音乐 爱好:游戏
```

按照以下方案设计特征组.

| 特征 | 特征组 | 特征组id | 子特征 | 子特征id | 特征id |
| - | - | - | - | - | - |
| 性别:男 | 性别 | 1 | 男 | 1 | (1 << 48) \| (1 & 0x0000ffffffffffff) |
| 性别:女 | 性别 | 1 | 女 | 2 | (1 << 48) \| (2 & 0x0000ffffffffffff) |
| 年龄:20 | 年龄 | 2 | 20 | 20 | (2 << 48) \| (20 & 0x0000ffffffffffff) |
| 年龄:25 | 年龄 | 2 | 25 | 25 | (2 << 48) \| (25 & 0x0000ffffffffffff) |
| 年龄:30 | 年龄 | 2 | 30 | 30 | (2 << 48) \| (30 & 0x0000ffffffffffff) |
| 爱好:乒乓球 | 爱好 | 3 | 乒乓球 | hash(乒乓球) | (3 << 48) \| (hash(乒乓球) & 0x0000ffffffffffff) |
| 爱好:音乐 | 爱好 | 3 | 音乐 | hash(音乐) | (3 << 48) \| (hash(音乐) & 0x0000ffffffffffff) |
| 爱好:游戏 | 爱好 | 3 | 游戏 | hash(游戏) | (3 << 48) \| (hash(游戏) & 0x0000ffffffffffff) |
| 爱好:篮球 | 爱好 | 3 | 篮球 | hash(篮球) | (3 << 48) \| (hash(篮球) & 0x0000ffffffffffff) |

特征组是性别, 年龄, 爱好. 为它们分配唯一的特征组id, 分别是1, 2, 3.

性别, 子特征是离散类别型, 子特征只有男和女, 子特征id可以分别编码为1和2.

年龄, 子特征是离散数值型, 子特征id可以使用它们的离散数值.

爱好, 子特征是离散类别型, 子特征空间是开放的, 子特征id使用哈希函数生成.

### 特征组配置

特征组配置包含所有特征组的<特征组id, embedding矩阵行, embedding矩阵列>信息.

fm族模型要求所有特征组的"embedding矩阵列"相同.

[排序模型](../example/rank/README.md)中, 大部分模型通过以下方式传入特征组配置.

```shell
--model_config="group_config=gc;sparse=s;..."
```

#### group\_config

如果gc是文件名, 对应的文件内容是.

```
特征组id1 embedding矩阵行1 embedding矩阵列1
特征组id2 embedding矩阵行2 embedding矩阵列2
...
```

"特征组例子"中对应的文件内容可以是.

```
1 2 16
2 100 16
3 1000000 32
```

如果gc不是文件名, 其内容是.

```
特征组id1:embedding矩阵行1:embedding矩阵列1,特征组id2:embedding矩阵行2:embedding矩阵列2,...
```

"特征组例子"中对应的内容可以是.

```
1:2:16,2:100:16,3:1000000:32
```

#### sparse

如果s是0, embedding矩阵是TSR, 其形状是(embedding矩阵行, embedding矩阵列).

不同特征id可能对应相同embedding, 即冲突. 为了减少冲突, 通常"embedding矩阵行"和"特征组的子特征空间"成正比.

如果s是1, embedding矩阵是SRM, 其形状是(0, embedding矩阵列), "embedding矩阵行"被忽略.

所有特征id独享自己的embedding.

## 特征值

deepx中, 特征值是浮点数.

为防止脏数据, 特征值的有效范围是[-100, 100].
