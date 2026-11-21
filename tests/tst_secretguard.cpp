// SPDX-License-Identifier: MIT
#include "ai/SecretGuard.h"

#include <QTest>

using namespace Qt::StringLiterals;

namespace {
// Token-shaped strings are assembled at run time so that no literal in the repository looks like a credential to
// secret scanners.
QByteArray tok(const char *prefix, int n, char fill = 'a')
{
    return QByteArray(prefix) + QByteArray(n, fill);
}
}

class TestSecretGuard : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void secretName_data();
    void secretName();
    void scanContent_data();
    void scanContent();
    void scanContentLimit();
    void looksBinary_data();
    void looksBinary();
};

void TestSecretGuard::secretName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("secret");

    const char *positives[] = {".env", ".ENV", ".env.local", ".env.production", "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519",
                               "id_ed25519_sk", "server.pem", "tls.key", "cert.p12", "cert.pfx", "release.jks", "app.keystore",
                               "vault.kdbx", "backup.gpg", "signing.asc", "office.ovpn", "credentials", "credentials.json",
                               "secret.json", "secrets.yaml", "secrets.yml", "secrets.toml", ".netrc", ".pgpass", ".npmrc",
                               ".pypirc", ".git-credentials", "service-account-prod.json", "service_account.json",
                               "github_token", "token.json", "auth.json"};
    for (const char *n : positives)
        QTest::newRow(n) << QString::fromLatin1(n) << true;

    const char *negatives[] = {".env.example", ".env.sample", ".env.template", "env", "id_rsa.pub", "README.md", "keyboard.txt",
                               "monkey.py", "credentials.txt", "secrets.md", "tokenizer.json", "token.txt", "author.json",
                               "service-account.yaml", "keys.csv"};
    for (const char *n : negatives)
        QTest::newRow(n) << QString::fromLatin1(n) << false;
}

void TestSecretGuard::secretName()
{
    QFETCH(QString, name);
    QFETCH(bool, secret);
    QCOMPARE(SecretGuard::isSecretName(name), secret);
}

void TestSecretGuard::scanContent_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("reason");

    const QByteArray begin = "-----BEGIN ";
    QTest::newRow("private key") << begin + "OPENSSH PRIVATE KEY-----\nabc" << u"private key"_s;
    QTest::newRow("rsa private key") << begin + "RSA PRIVATE KEY-----" << u"private key"_s;
    QTest::newRow("public key") << begin + "PUBLIC KEY-----\nabc" << QString();

    QTest::newRow("aws") << "key = " + tok("AKIA", 16, 'Q') << u"AWS key"_s;
    QTest::newRow("aws short") << "key = " + tok("AKIA", 15, 'Q') << QString();
    QTest::newRow("aws lowercase") << "key = " + tok("AKIA", 16, 'q') << QString();

    for (const char *p : {"ghp_", "gho_", "ghu_", "ghs_", "ghr_"})
        QTest::newRow(p) << "t=" + tok(p, 36) << u"GitHub token"_s;
    QTest::newRow("gh short") << "t=" + tok("ghp_", 35) << QString();
    QTest::newRow("gh other letter") << "t=" + tok("ghx_", 36) << QString();
    QTest::newRow("gh inside word") << "t=x" + tok("ghp_", 36) << QString();

    QTest::newRow("github_pat") << "t=" + tok("github_pat_", 40) << u"GitHub token"_s;
    QTest::newRow("github_pat short") << "t=" + tok("github_pat_", 39) << QString();

    QTest::newRow("sk") << "OPENAI=" + tok("sk-", 24) << u"API key (OpenAI/Anthropic)"_s;
    QTest::newRow("sk-ant") << "x " + tok("sk-ant-", 24) << u"API key (OpenAI/Anthropic)"_s;
    QTest::newRow("sk-proj") << "x " + tok("sk-proj-", 24) << u"API key (OpenAI/Anthropic)"_s;
    QTest::newRow("sk short") << "x " + tok("sk-", 23) << QString();
    QTest::newRow("sk inside word") << "x " + tok("task-", 30) << QString();

    for (const char *p : {"xoxb-", "xoxa-", "xoxp-", "xoxr-", "xoxs-"})
        QTest::newRow(p) << "slack " + tok(p, 10) << u"Slack token"_s;
    QTest::newRow("slack short") << "slack " + tok("xoxb-", 9) << QString();
    QTest::newRow("slack other letter") << "slack " + tok("xoxq-", 10) << QString();

    QTest::newRow("google") << "k=" + tok("AIza", 35) << u"Google API key"_s;
    QTest::newRow("google short") << "k=" + tok("AIza", 34) << QString();

    QTest::newRow("hf") << "HF=" + tok("hf_", 30) << u"Hugging Face token"_s;
    QTest::newRow("hf short") << "HF=" + tok("hf_", 29) << QString();

    for (const char *s : {"postgres", "postgresql", "mysql", "mongodb", "mongodb+srv"})
        QTest::newRow(s) << QByteArray(s) + "://app:hunter2@db.local/x" << u"URL with a password"_s;
    QTest::newRow("url without password") << QByteArrayLiteral("postgres://app@db.local/x") << QString();
    QTest::newRow("url other scheme") << QByteArrayLiteral("https://app:hunter2@example.org/") << QString();

    QTest::newRow("plain text") << QByteArrayLiteral("Just some notes about tokens and keys.\n") << QString();
    QTest::newRow("empty") << QByteArray() << QString();
}

void TestSecretGuard::scanContent()
{
    QFETCH(QByteArray, data);
    QFETCH(QString, reason);
    QCOMPARE(SecretGuard::scanContent(data), reason);
}

void TestSecretGuard::scanContentLimit()
{
    // Only the first 512 KiB are scanned, which keeps huge files cheap.
    const QByteArray filler(512 * 1024, 'x');
    const QByteArray token = '\n' + tok("hf_", 30);
    QCOMPARE(SecretGuard::scanContent(filler.left(1000) + token), u"Hugging Face token"_s);
    QCOMPARE(SecretGuard::scanContent(filler + token), QString());
}

void TestSecretGuard::looksBinary_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<bool>("binary");

    QTest::newRow("empty") << QByteArray() << false;
    QTest::newRow("ascii") << QByteArrayLiteral("hello\nworld\n") << false;
    QTest::newRow("utf-8") << QByteArrayLiteral("Fūin 封印 ✓\n") << false;
    QTest::newRow("nul") << QByteArray("ab\0cd", 5) << true;
    QTest::newRow("invalid utf-8") << QByteArrayLiteral("abc\xff\xfe") << true;
    // A multi-byte character cut by the 8 KiB probe is not an encoding error.
    QTest::newRow("cut at probe end") << QByteArray(8191, 'a') + "é and more" << false;
    QTest::newRow("nul after probe") << QByteArray(8192, 'a') + QByteArray(1, '\0') << false;
}

void TestSecretGuard::looksBinary()
{
    QFETCH(QByteArray, data);
    QFETCH(bool, binary);
    QCOMPARE(SecretGuard::looksBinary(data), binary);
}

QTEST_GUILESS_MAIN(TestSecretGuard)
#include "tst_secretguard.moc"
