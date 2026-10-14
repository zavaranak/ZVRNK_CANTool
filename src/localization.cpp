#include "localization.h"

#include <QHash>

QString localize(AppLanguage language, const QString& source)
{
    if (language == AppLanguage::English || source.isEmpty()) {
        return source;
    }

    static const QHash<QString, QString> translations = {
        {QStringLiteral("View"), QStringLiteral("Hiển thị")},
        {QStringLiteral("System theme"), QStringLiteral("Chủ đề hệ thống")},
        {QStringLiteral("Light theme"), QStringLiteral("Chủ đề sáng")},
        {QStringLiteral("Dark theme"), QStringLiteral("Chủ đề tối")},
        {QStringLiteral("Language"), QStringLiteral("Ngôn ngữ")},
        {QStringLiteral("English"), QStringLiteral("English")},
        {QStringLiteral("Vietnamese"), QStringLiteral("Tiếng Việt")},
        {QStringLiteral("Language menu"), QStringLiteral("Menu ngôn ngữ")},
        {QStringLiteral("Language selector"), QStringLiteral("Bộ chọn ngôn ngữ")},
        {QStringLiteral("Choose English or Vietnamese"), QStringLiteral("Chọn English hoặc Tiếng Việt")},
        {QStringLiteral("ZVRNK CAN Tool"), QStringLiteral("Công cụ CAN ZVRNK")},
        {QStringLiteral("+ Add Session"), QStringLiteral("+ Thêm phiên")},
        {QStringLiteral("ZVRNK TOOL FAMILY / CAN BUS TOOL"), QStringLiteral("HỌ CÔNG CỤ ZVRNK / CÔNG CỤ CAN")},
        {QStringLiteral("Configure, transmit, and inspect live CAN traffic."), QStringLiteral("Cấu hình, truyền và giám sát lưu lượng CAN trực tiếp.")},
        {QStringLiteral("Profile"), QStringLiteral("Hồ sơ")},
        {QStringLiteral("Device"), QStringLiteral("Thiết bị")},
        {QStringLiteral("Channel"), QStringLiteral("Kênh")},
        {QStringLiteral("Baud"), QStringLiteral("Tốc độ")},
        {QStringLiteral("Mode"), QStringLiteral("Chế độ")},
        {QStringLiteral("Filter"), QStringLiteral("Bộ lọc")},
        {QStringLiteral("Fault Reading"), QStringLiteral("Đọc lỗi")},
        {QStringLiteral("Normal"), QStringLiteral("Bình thường")},
        {QStringLiteral("Listen Only"), QStringLiteral("Chỉ nghe")},
        {QStringLiteral("Accept All"), QStringLiteral("Nhận tất cả")},
        {QStringLiteral("Single Filter"), QStringLiteral("Bộ lọc đơn")},
        {QStringLiteral("Connect"), QStringLiteral("Kết nối")},
        {QStringLiteral("Disconnect"), QStringLiteral("Ngắt kết nối")},
        {QStringLiteral("Refresh"), QStringLiteral("Làm mới")},
        {QStringLiteral("Close Session"), QStringLiteral("Đóng phiên")},
        {QStringLiteral("DISCONNECTED"), QStringLiteral("ĐÃ NGẮT KẾT NỐI")},
        {QStringLiteral("CONNECTED"), QStringLiteral("ĐÃ KẾT NỐI")},
        {QStringLiteral("NMEA / LISTEN ONLY"), QStringLiteral("NMEA / CHỈ NGHE")},
        {QStringLiteral("Transmit"), QStringLiteral("Truyền")},
        {QStringLiteral("Transmit (disabled in NMEA 2000)"), QStringLiteral("Truyền (tắt trong NMEA 2000)")},
        {QStringLiteral("Received Frames"), QStringLiteral("Khung đã nhận")},
        {QStringLiteral("Event Log"), QStringLiteral("Nhật ký sự kiện")},
        {QStringLiteral("Add"), QStringLiteral("Thêm")},
        {QStringLiteral("Remove"), QStringLiteral("Xóa")},
        {QStringLiteral("Send"), QStringLiteral("Gửi")},
        {QStringLiteral("Cycle"), QStringLiteral("Chu kỳ")},
        {QStringLiteral("Stop"), QStringLiteral("Dừng")},
        {QStringLiteral("Loop Queue"), QStringLiteral("Vòng lặp hàng đợi")},
        {QStringLiteral("Stop Queue"), QStringLiteral("Dừng hàng đợi")},
        {QStringLiteral("Export CSV"), QStringLiteral("Xuất CSV")},
        {QStringLiteral("Import CSV"), QStringLiteral("Nhập CSV")},
        {QStringLiteral("Extended Frame"), QStringLiteral("Khung mở rộng")},
        {QStringLiteral("Remote Frame"), QStringLiteral("Khung yêu cầu")},
        {QStringLiteral("Actions"), QStringLiteral("Tác vụ")},
        {QStringLiteral("Name"), QStringLiteral("Tên")},
        {QStringLiteral("Ext"), QStringLiteral("Mở rộng")},
        {QStringLiteral("RTR"), QStringLiteral("RTR")},
        {QStringLiteral("Interval(ms)"), QStringLiteral("Chu kỳ (ms)")},
        {QStringLiteral("ID"), QStringLiteral("ID")},
        {QStringLiteral("Data"), QStringLiteral("Dữ liệu")},
        {QStringLiteral("Watch"), QStringLiteral("Theo dõi")},
        {QStringLiteral("Clear List"), QStringLiteral("Xóa danh sách")},
        {QStringLiteral("Group Unique"), QStringLiteral("Nhóm duy nhất")},
        {QStringLiteral("Reset"), QStringLiteral("Đặt lại")},
        {QStringLiteral("Filter ID"), QStringLiteral("Lọc ID")},
        {QStringLiteral("Payload Contains"), QStringLiteral("Payload chứa")},
        {QStringLiteral("ALL"), QStringLiteral("TẤT CẢ")},
        {QStringLiteral("Signed"), QStringLiteral("Có dấu")},
        {QStringLiteral("Watch ID"), QStringLiteral("ID theo dõi")},
        {QStringLiteral("Start Byte"), QStringLiteral("Byte bắt đầu")},
        {QStringLiteral("Length"), QStringLiteral("Độ dài")},
        {QStringLiteral("Endian"), QStringLiteral("Thứ tự byte")},
        {QStringLiteral("Little"), QStringLiteral("Thấp trước")},
        {QStringLiteral("Big"), QStringLiteral("Cao trước")},
        {QStringLiteral("Formula (x = dec)"), QStringLiteral("Công thức (x = thập phân)")},
        {QStringLiteral("Raw Value (dec)"), QStringLiteral("Giá trị thô (thập phân)")},
        {QStringLiteral("Computed Value"), QStringLiteral("Giá trị tính toán")},
        {QStringLiteral("Dev"), QStringLiteral("Thiết bị")},
        {QStringLiteral("Disconnected"), QStringLiteral("Đã ngắt kết nối")},
        {QStringLiteral("Invalid config"), QStringLiteral("Cấu hình không hợp lệ")},
        {QStringLiteral("Out of range"), QStringLiteral("Ngoài phạm vi")},
        {QStringLiteral("No match"), QStringLiteral("Không khớp")},
        {QStringLiteral("Select the adapter operating profile. NMEA 2000 is receive-only."), QStringLiteral("Chọn cấu hình bộ chuyển đổi. NMEA 2000 chỉ nhận dữ liệu.")},
        {QStringLiteral("NMEA 2000 monitor: 250 kbit/s, extended IDs, accept all, listen-only"), QStringLiteral("Giám sát NMEA 2000: 250 kbit/s, ID mở rộng, nhận tất cả, chỉ nghe")},
        {QStringLiteral("Fault reading profile: use the selected adapter settings"), QStringLiteral("Cấu hình đọc lỗi: dùng thiết lập bộ chuyển đổi đã chọn")},
    };

    if (translations.contains(source)) {
        return translations.value(source);
    }

    if (source.startsWith(QStringLiteral("Session "))) {
        QString result = source;
        result.replace(QStringLiteral("Session "), QStringLiteral("Phiên "));
        result.replace(QStringLiteral("Disconnected"), QStringLiteral("Đã ngắt kết nối"));
        result.replace(QStringLiteral("Dev "), QStringLiteral("Thiết bị "));
        result.replace(QStringLiteral(" / CH "), QStringLiteral(" / Kênh "));
        return result;
    }

    return source;
}
