﻿#include "SACsvWriter.h"
#include <QTextStream>
#include <QFile>
class SACsvWriterPrivate
{
    SACsvWriter* Parent;
    QTextStream* m_txt;
    QScopedPointer<QTextStream> m_pfile;
    bool m_isStartLine;
public:
    SACsvWriterPrivate(SACsvWriter* p):Parent(p)
      ,m_txt(nullptr)
      ,m_pfile(nullptr)
      ,m_isStartLine(true)
    {

    }
    void setTextStream(QTextStream* st)
    {
        m_txt = st;
    }
    QTextStream* stream()
    {
        return m_txt;
    }
    QTextStream& streamRef()
    {
        return (*m_txt);
    }
    bool isStartLine() const
    {
        return m_isStartLine;
    }
    void setStartLine(bool on)
    {
        m_isStartLine = on;
    }

    void setFile(QFile *txt)
    {
        m_pfile.reset(new QTextStream(txt));
        m_txt = m_pfile.data();
    }

    QTextStream& formatTextStream()
    {
        if(!isStartLine())
        {
            streamRef()<<',';
        }
        else
        {
            setStartLine(false);
        }
        return streamRef();
    }
};



SACsvWriter::SACsvWriter(QTextStream* txt)
    :d_p(new SACsvWriterPrivate(this))
{
    d_p->setTextStream(txt);
}

SACsvWriter::SACsvWriter(QFile *txt)
    :d_p(new SACsvWriterPrivate(this))
{
    d_p->setFile(txt);
}

SACsvWriter::~SACsvWriter()
{

}
///
/// \brief 把字符串装换为标准csv一个单元得字符串，对应字符串原有的逗号会进行装换
///
/// csv的原则是：
///
/// - 如果字符串有逗号，把整个字符串前后用引号括起来
/// - 如果字符串有引号",引号要用两个引号表示转义""
/// \param rawStr 原有数据
/// \return 标准的csv单元字符串
///
QString SACsvWriter::toCsvString(const QString &rawStr)
{
    //首先查找有没有引号,如果有，则把引号替换为两个引号
    QString res = rawStr;
    res.replace("\"","\"\"");
    if(res.contains(','))
    {
        //如果有逗号，在前后把整个句子用引号包含
        res = ('\"' + res + '\"');
    }
    return res;
}
///
/// \brief 把一行要用逗号分隔的字符串转换为一行标准csv字符串
/// \param sectionLine 如：xxx,xxxx,xxxxx 传入{'xxx','xxxx','xxxxx'}
/// \return 标准的csv字符串不带换行符
///
QString SACsvWriter::toCsvStringLine(const QStringList &sectionLine)
{
    QString res;
    int size = sectionLine.size();
    for(int i=0;i<size;++i)
    {
        if(0 == i)
        {
            res += SACsvWriter::toCsvString(sectionLine[i]);
        }
        else
        {
            res += ("," + SACsvWriter::toCsvString(sectionLine[i]));
        }
    }
    return res;
}
///
/// \brief 写csv文件内容，字符会自动转义为csv文件支持的字符串，不需要转义
///
/// 例如csv文件：
/// \par
/// 12,txt,23,34
/// 45,num,56,56
/// 写入的方法为：
/// \code
/// .....
/// QCsvWriter csv(&textStream);
/// csv<<"12"<<"txt"<<"23";
/// csv.endLine("34");
/// csv<<"45"<<"num"<<"56";
/// csv.endLine("56");
/// \endcode
///
/// \param str 需要写入的csv文件一个单元得字符串
/// \return
///
SACsvWriter &SACsvWriter::operator <<(const QString &str)
{
    d_p->formatTextStream()<<toCsvString(str);
    return *this;
}

SACsvWriter &SACsvWriter::operator <<(int d)
{
    d_p->formatTextStream()<<d;
    return *this;
}

SACsvWriter &SACsvWriter::operator <<(double d)
{
    d_p->formatTextStream()<<d;
    return *this;
}

SACsvWriter &SACsvWriter::operator <<(float d)
{
    d_p->formatTextStream()<<d;
    return *this;
}

///
/// \brief 换行
///
void SACsvWriter::newLine()
{
    d_p->setStartLine(true);
    d_p->streamRef()<<endl;
}

QTextStream *SACsvWriter::stream() const
{
    return d_p->stream();
}

///
/// \brief endl
/// \param s
/// \return
///
SACsvWriter &endl(SACsvWriter &s)
{
    s.newLine();
    return s;
}
