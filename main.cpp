#include <QtGui>
#include <vector>
#include <clang-c/Index.h>

class Editor : public QWidget
{
    Q_OBJECT
public:
    Editor(QWidget* parent = 0);
    ~Editor();

private slots:
    void onTextChanged();
    void onCompleteClicked();

private:
    QTextEdit* edit;
    QPushButton* completeButton;

    CXIndex idx;
    CXTranslationUnit unit;
};


Editor::Editor(QWidget* parent)
    : QWidget(parent), unit(0)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    edit = new QTextEdit(this);
    layout->addWidget(edit);
    completeButton = new QPushButton(this);
    completeButton->setText("Complete");
    layout->addWidget(completeButton);

    connect(edit, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    connect(completeButton, SIGNAL(clicked()), this, SLOT(onCompleteClicked()));

    idx = clang_createIndex(1, 0);
}

Editor::~Editor()
{
    if (unit)
        clang_disposeTranslationUnit(unit);
    clang_disposeIndex(idx);
}

static void warnDiag(const char* prefix, CXDiagnostic diag)
{
    CXString str = clang_formatDiagnostic(diag, CXDiagnostic_DisplaySourceLocation|CXDiagnostic_DisplayColumn);
    qWarning("%s: %s", prefix, clang_getCString(str));
    clang_disposeString(str);
    clang_disposeDiagnostic(diag);
}

void Editor::onCompleteClicked()
{
    QByteArray data = edit->toPlainText().toUtf8();

    CXUnsavedFile sourceFile;
    sourceFile.Filename = "main.cpp";
    sourceFile.Contents = data.constData();
    sourceFile.Length = data.size();

    QTextCursor cursor = edit->textCursor();
    const int line = cursor.block().firstLineNumber() + 1;
    const int column = cursor.positionInBlock();

    qWarning("completing at %d, %d", line, column);

    CXCodeCompleteResults* complete = clang_codeCompleteAt(unit, "main.cpp", line, column,
                                                           &sourceFile, 1,
                                                           clang_defaultCodeCompleteOptions());
    if (!complete) {
        qWarning("no complete results");
        return;
    }
    unsigned numDiags = clang_codeCompleteGetNumDiagnostics(complete);
    for (unsigned i = 0; i < numDiags; ++i) {
        CXDiagnostic diag = clang_codeCompleteGetDiagnostic(complete, i);
        warnDiag("complete diag", diag);
    }
    for (unsigned i = 0; i < complete->NumResults; ++i) {
        CXCompletionResult& result = complete->Results[i];
        CXCursorKind& kind = result.CursorKind;
        CXCompletionString& string = result.CompletionString;
        unsigned numChunks = clang_getNumCompletionChunks(string);
        for (unsigned j = 0; j < numChunks; ++j) {
            CXCompletionChunkKind chunkKind = clang_getCompletionChunkKind(string, j);
            if (chunkKind == CXCompletionChunk_TypedText) {
                CXString text = clang_getCompletionChunkText(string, j);
                qWarning("complete result, kind: %d, text: %s", kind, clang_getCString(text));
                clang_disposeString(text);
            }
        }
    }
    clang_disposeCodeCompleteResults(complete);
}

void Editor::onTextChanged()
{
    QByteArray data = edit->toPlainText().toUtf8();

    CXUnsavedFile sourceFile;
    sourceFile.Filename = "main.cpp";
    sourceFile.Contents = data.constData();
    sourceFile.Length = data.size();
    if (!unit) {
        // make one
        std::vector<const char*> compileArgs;
        compileArgs.push_back("-I/usr/include");
        compileArgs.push_back("-I/usr/include/qt4");
        compileArgs.push_back("-I/usr/include/qt4/QtGui");
        compileArgs.push_back("-c");
        unit = clang_parseTranslationUnit(idx, "main.cpp", compileArgs.data(), compileArgs.size(), &sourceFile, 1,
                                          CXTranslationUnit_PrecompiledPreamble|CXTranslationUnit_CacheCompletionResults);
        if (!unit) {
            qWarning("failed to parse translation unit");
        }
    } else {
        int failure = clang_reparseTranslationUnit(unit, 1, &sourceFile, clang_defaultReparseOptions(unit));
        if (failure) {
            clang_disposeTranslationUnit(unit);
            unit = 0;
            qWarning("failed to reparse translation unit");
        }
    }

    unsigned numDiags = clang_getNumDiagnostics(unit);
    for (unsigned i = 0; i < numDiags; ++i) {
        CXDiagnostic diag = clang_getDiagnostic(unit, i);
        warnDiag("compile diag", diag);
    }
    if (!numDiags)
        qWarning("all ok");
}

#include "main.moc"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    Editor editor;
    editor.resize(300, 300);
    editor.show();

    return app.exec();
}

