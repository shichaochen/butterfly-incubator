# GitHub同步指南

本地代码已提交到Git仓库。现在需要将其推送到GitHub。

## 方法一：使用GitHub CLI（推荐）

如果您已安装GitHub CLI：

```bash
# 在GitHub上创建新仓库并推送
gh repo create butterfly-incubator --public --source=. --remote=origin --push
```

## 方法二：手动创建仓库

### 步骤1：在GitHub上创建新仓库

1. 访问 https://github.com/new
2. 仓库名称：`butterfly-incubator`（或您喜欢的名称）
3. 选择 **Public** 或 **Private**
4. **不要**勾选"Initialize this repository with a README"
5. 点击 **Create repository**

### 步骤2：添加远程仓库并推送

**如果您创建的是新仓库，使用以下命令：**

```bash
# 替换 YOUR_USERNAME 为您的GitHub用户名
# 替换 REPO_NAME 为您创建的仓库名称
git remote add origin https://github.com/YOUR_USERNAME/REPO_NAME.git
git branch -M main
git push -u origin main
```

**如果您使用SSH（推荐）：**

```bash
git remote add origin git@github.com:YOUR_USERNAME/REPO_NAME.git
git branch -M main
git push -u origin main
```

## 方法三：使用现有仓库

如果您已经有GitHub仓库，只需添加远程地址：

```bash
# HTTPS方式
git remote add origin https://github.com/YOUR_USERNAME/REPO_NAME.git

# 或SSH方式
git remote add origin git@github.com:YOUR_USERNAME/REPO_NAME.git

# 然后推送
git branch -M main
git push -u origin main
```

## 后续更新

推送代码后，以后更新代码只需：

```bash
git add .
git commit -m "更新说明"
git push
```

## 常见问题

### 认证问题

如果推送时要求输入用户名密码：
- GitHub已不再支持密码认证
- 使用Personal Access Token（PAT）代替密码
- 或配置SSH密钥（推荐）

### 配置SSH密钥

```bash
# 生成SSH密钥（如果还没有）
ssh-keygen -t ed25519 -C "your_email@example.com"

# 复制公钥
cat ~/.ssh/id_ed25519.pub

# 将公钥添加到GitHub: Settings → SSH and GPG keys → New SSH key
```

### 分支名称

如果您的默认分支是`master`而不是`main`：

```bash
git branch -M main  # 重命名分支
# 或
git push -u origin master  # 使用master分支
```

