#include <streambuf>
#include <istream>

class LimitedIStream : public std::istream
{
public:
    LimitedIStream(std::unique_ptr<std::istream> src, size_t limit)
        : std::istream(new LimitedStreambuf(src->rdbuf(), limit)),
          m_src(std::move(src))
    {
        // 接管原始流的缓冲区
    }

    ~LimitedIStream()
    {
        delete rdbuf(); // 清理自定义的缓冲区
    }

private:
    std::unique_ptr<std::istream> m_src;

    // 自定义缓冲区类
    class LimitedStreambuf : public std::streambuf
    {
    public:
        LimitedStreambuf(std::streambuf *src, size_t limit)
            : m_src(src), m_remaining(limit) {}

    protected:
        // 重载读取字符的逻辑
        int_type underflow() override
        {
            if (m_remaining <= 0)
            {
                return traits_type::eof();
            }

            // 从原始流读取一个字符
            int_type ch = m_src->sbumpc();
            if (ch != traits_type::eof())
            {
                m_remaining--;
                // 将字符放回缓冲区供后续读取
                this->setg(&m_current_char, &m_current_char, &m_current_char + 1);
                m_current_char = traits_type::to_char_type(ch);
            }
            return ch;
        }

    private:
        std::streambuf *m_src;
        size_t m_remaining;
        char m_current_char;
    };
};