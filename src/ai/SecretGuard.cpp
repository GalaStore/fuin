// SPDX-License-Identifier: MIT
#include "SecretGuard.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringDecoder>

bool SecretGuard::isSecretName(const QString &fileName)
{
    const QString n = fileName.toLower();
    if (n == QLatin1String(".env") || (n.startsWith(QLatin1String(".env.")) && !n.endsWith(QLatin1String(".example"))
                                       && !n.endsWith(QLatin1String(".sample")) && !n.endsWith(QLatin1String(".template"))))
        return true;
    static const QRegularExpression re(QStringLiteral(
        R"(^(id_(rsa|dsa|ecdsa|ed25519)(_sk)?|.*\.(pem|key|p12|pfx|jks|keystore|kdbx|gpg|asc|ovpn)|credentials(\.json)?|secrets?\.(json|ya?ml|toml)|)"
        R"(\.netrc|\.pgpass|\.npmrc|\.pypirc|\.git-credentials|service[-_]account.*\.json|.*_token|token\.json|auth\.json)$)"));
    return re.match(n).hasMatch();
}

QString SecretGuard::scanContent(const QByteArray &data)
{
    struct Pat {
        const char *re;
        const char *label;
    };
    static const Pat pats[] = {
        {R"(-----BEGIN [A-Z ]*PRIVATE KEY-----)", QT_TRANSLATE_NOOP("SecretGuard", "private key")},
        {R"(AKIA[0-9A-Z]{16})", QT_TRANSLATE_NOOP("SecretGuard", "AWS key")},
        {R"(\bgh[pousr]_[A-Za-z0-9]{36,})", QT_TRANSLATE_NOOP("SecretGuard", "GitHub token")},
        {R"(\bgithub_pat_[A-Za-z0-9_]{40,})", QT_TRANSLATE_NOOP("SecretGuard", "GitHub token")},
        {R"(\bsk-(ant-|proj-)?[A-Za-z0-9_\-]{24,})", QT_TRANSLATE_NOOP("SecretGuard", "API key (OpenAI/Anthropic)")},
        {R"(\bxox[baprs]-[A-Za-z0-9\-]{10,})", QT_TRANSLATE_NOOP("SecretGuard", "Slack token")},
        {R"(\bAIza[0-9A-Za-z_\-]{35})", QT_TRANSLATE_NOOP("SecretGuard", "Google API key")},
        {R"(\bhf_[A-Za-z0-9]{30,})", QT_TRANSLATE_NOOP("SecretGuard", "Hugging Face token")},
        {R"(\b(postgres|postgresql|mysql|mongodb(\+srv)?)://[^\s:/]+:[^\s@/]+@)", QT_TRANSLATE_NOOP("SecretGuard", "URL with a password")},
    };
    static QList<QPair<QRegularExpression, const char *>> compiled = [] {
        QList<QPair<QRegularExpression, const char *>> out;
        for (const Pat &p : pats)
            out.append({QRegularExpression(QString::fromLatin1(p.re)), p.label});
        return out;
    }();
    const QString text = QString::fromUtf8(data.left(512 * 1024));
    for (const auto &c : std::as_const(compiled))
        if (c.first.match(text).hasMatch())
            return QCoreApplication::translate("SecretGuard", c.second);
    return {};
}

bool SecretGuard::looksBinary(const QByteArray &data)
{
    const QByteArray head = data.left(8192);
    if (head.contains('\0'))
        return true;
    // ignore a multi-byte sequence cut at the end of the probe
    const QByteArray probe = head.size() == 8192 ? head.left(8188) : head;
    QStringDecoder dec(QStringDecoder::Utf8);
    const QString decoded = dec.decode(probe);
    Q_UNUSED(decoded)
    return dec.hasError();
}
