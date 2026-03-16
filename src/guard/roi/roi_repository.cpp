#include "catcheye/guard/roi/roi_repository.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace catcheye::guard::roi {
namespace {

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Variant value;

    [[nodiscard]] bool is_object() const { return std::holds_alternative<JsonObject>(value); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<JsonArray>(value); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(value); }
    [[nodiscard]] bool is_boolean() const { return std::holds_alternative<bool>(value); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(value); }

    [[nodiscard]] const JsonObject& as_object() const { return std::get<JsonObject>(value); }
    [[nodiscard]] const JsonArray& as_array() const { return std::get<JsonArray>(value); }
    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(value); }
    [[nodiscard]] bool as_bool() const { return std::get<bool>(value); }
    [[nodiscard]] double as_number() const { return std::get<double>(value); }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    JsonValue parse()
    {
        skip_ws();
        JsonValue value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            throw std::runtime_error("unexpected trailing characters");
        }
        return value;
    }

private:
    JsonValue parse_value()
    {
        if (pos_ >= text_.size()) {
            throw std::runtime_error("unexpected end of input");
        }

        const char ch = text_[pos_];
        if (ch == '{') return parse_object();
        if (ch == '[') return parse_array();
        if (ch == '"') return JsonValue {parse_string()};
        if (ch == 't') return parse_true();
        if (ch == 'f') return parse_false();
        if (ch == 'n') return parse_null();
        if ((ch == '-') || std::isdigit(static_cast<unsigned char>(ch))) return JsonValue {parse_number()};

        throw std::runtime_error("invalid json value");
    }

    JsonValue parse_object()
    {
        expect('{');
        skip_ws();
        JsonObject obj;
        if (peek('}')) {
            expect('}');
            return JsonValue {obj};
        }

        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            obj.emplace(std::move(key), parse_value());
            skip_ws();
            if (peek('}')) {
                expect('}');
                break;
            }
            expect(',');
            skip_ws();
        }

        return JsonValue {obj};
    }

    JsonValue parse_array()
    {
        expect('[');
        skip_ws();
        JsonArray arr;

        if (peek(']')) {
            expect(']');
            return JsonValue {arr};
        }

        while (true) {
            arr.push_back(parse_value());
            skip_ws();
            if (peek(']')) {
                expect(']');
                break;
            }
            expect(',');
            skip_ws();
        }

        return JsonValue {arr};
    }

    std::string parse_string()
    {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            char ch = text_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    throw std::runtime_error("unterminated escape sequence");
                }
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: throw std::runtime_error("unsupported escape sequence");
                }
                continue;
            }
            out.push_back(ch);
        }

        throw std::runtime_error("unterminated string");
    }

    double parse_number()
    {
        const std::size_t start = pos_;
        if (peek('-')) {
            ++pos_;
        }

        if (peek('0')) {
            ++pos_;
        } else {
            parse_digits();
        }

        if (peek('.')) {
            ++pos_;
            parse_digits();
        }

        if (peek('e') || peek('E')) {
            ++pos_;
            if (peek('+') || peek('-')) {
                ++pos_;
            }
            parse_digits();
        }

        return std::stod(text_.substr(start, pos_ - start));
    }

    JsonValue parse_true()
    {
        expect_sequence("true");
        return JsonValue {true};
    }

    JsonValue parse_false()
    {
        expect_sequence("false");
        return JsonValue {false};
    }

    JsonValue parse_null()
    {
        expect_sequence("null");
        return JsonValue {nullptr};
    }

    void parse_digits()
    {
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            throw std::runtime_error("invalid number");
        }

        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    void skip_ws()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool peek(char expected) const
    {
        return pos_ < text_.size() && text_[pos_] == expected;
    }

    void expect(char expected)
    {
        if (!peek(expected)) {
            throw std::runtime_error(std::string("expected '") + expected + "'");
        }
        ++pos_;
    }

    void expect_sequence(const char* sequence)
    {
        while (*sequence != '\0') {
            if (pos_ >= text_.size() || text_[pos_] != *sequence) {
                throw std::runtime_error("unexpected token");
            }
            ++pos_;
            ++sequence;
        }
    }

    const std::string& text_;
    std::size_t pos_ {0};
};

const JsonValue* get_member(const JsonObject& obj, const std::string& key)
{
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return &it->second;
}

bool number_to_int(double in, int& out)
{
    const double truncated = std::trunc(in);
    if (truncated != in) {
        return false;
    }
    out = static_cast<int>(in);
    return true;
}

RoiConfigParseResult parse_config_json(const JsonValue& root)
{
    RoiConfigParseResult result;
    if (!root.is_object()) {
        result.errors.push_back("root must be a JSON object");
        return result;
    }

    const auto& root_obj = root.as_object();

    const JsonValue* camera_id = get_member(root_obj, "camera_id");
    if (camera_id == nullptr || !camera_id->is_string()) {
        result.errors.push_back("camera_id is required and must be a string");
    } else {
        result.config.camera_id = camera_id->as_string();
    }

    const JsonValue* image_width = get_member(root_obj, "image_width");
    int parsed_width = 0;
    if (image_width == nullptr || !image_width->is_number() || !number_to_int(image_width->as_number(), parsed_width)) {
        result.errors.push_back("image_width is required and must be an integer");
    } else {
        result.config.image_width = parsed_width;
    }

    const JsonValue* image_height = get_member(root_obj, "image_height");
    int parsed_height = 0;
    if (image_height == nullptr || !image_height->is_number() || !number_to_int(image_height->as_number(), parsed_height)) {
        result.errors.push_back("image_height is required and must be an integer");
    } else {
        result.config.image_height = parsed_height;
    }

    const JsonValue* allowed_zones = get_member(root_obj, "allowed_zones");
    if (allowed_zones == nullptr || !allowed_zones->is_array()) {
        result.errors.push_back("allowed_zones is required and must be an array");
    } else {
        const auto& zones = allowed_zones->as_array();
        result.config.allowed_zones.reserve(zones.size());

        for (std::size_t zone_index = 0; zone_index < zones.size(); ++zone_index) {
            const JsonValue& zone_value = zones[zone_index];
            if (!zone_value.is_object()) {
                result.errors.push_back("allowed_zones[" + std::to_string(zone_index) + "] must be an object");
                continue;
            }

            const auto& zone_obj = zone_value.as_object();
            RoiPolygon zone;

            const JsonValue* id = get_member(zone_obj, "id");
            if (id == nullptr || !id->is_string()) {
                result.errors.push_back("allowed_zones[" + std::to_string(zone_index) + "].id must be a string");
            } else {
                zone.id = id->as_string();
            }

            const JsonValue* name = get_member(zone_obj, "name");
            if (name == nullptr || !name->is_string()) {
                result.errors.push_back("allowed_zones[" + std::to_string(zone_index) + "].name must be a string");
            } else {
                zone.name = name->as_string();
            }

            const JsonValue* enabled = get_member(zone_obj, "enabled");
            if (enabled == nullptr || !enabled->is_boolean()) {
                result.errors.push_back("allowed_zones[" + std::to_string(zone_index) + "].enabled must be a boolean");
            } else {
                zone.enabled = enabled->as_bool();
            }

            const JsonValue* points = get_member(zone_obj, "points");
            if (points == nullptr || !points->is_array()) {
                result.errors.push_back("allowed_zones[" + std::to_string(zone_index) + "].points must be an array");
            } else {
                const auto& points_array = points->as_array();
                zone.points.reserve(points_array.size());

                for (std::size_t point_index = 0; point_index < points_array.size(); ++point_index) {
                    const JsonValue& point = points_array[point_index];
                    if (!point.is_array()) {
                        result.errors.push_back(
                            "allowed_zones[" + std::to_string(zone_index) + "].points[" + std::to_string(point_index)
                            + "] must be [x, y]"
                        );
                        continue;
                    }

                    const auto& pair = point.as_array();
                    if (pair.size() != 2 || !pair[0].is_number() || !pair[1].is_number()) {
                        result.errors.push_back(
                            "allowed_zones[" + std::to_string(zone_index) + "].points[" + std::to_string(point_index)
                            + "] must be [x, y]"
                        );
                        continue;
                    }

                    zone.points.push_back(Point {pair[0].as_number(), pair[1].as_number()});
                }
            }

            result.config.allowed_zones.push_back(std::move(zone));
        }
    }

    result.success = result.errors.empty();
    return result;
}

std::string escape_json_string(const std::string& input)
{
    std::string escaped;
    escaped.reserve(input.size());

    for (const char ch : input) {
        switch (ch) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }

    return escaped;
}

void write_indent(std::ostringstream& oss, int indent, int level)
{
    for (int i = 0; i < indent * level; ++i) {
        oss << ' ';
    }
}

} // namespace

RoiConfigParseResult RoiRepository::from_json_string(const std::string& json_text)
{
    try {
        JsonParser parser(json_text);
        return parse_config_json(parser.parse());
    } catch (const std::exception& ex) {
        RoiConfigParseResult result;
        result.errors.push_back(std::string("json parse error: ") + ex.what());
        return result;
    }
}

RoiConfigParseResult RoiRepository::load_from_file(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        RoiConfigParseResult result;
        result.errors.push_back("failed to open file: " + path);
        return result;
    }

    std::string json_text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return from_json_string(json_text);
}

std::string RoiRepository::to_json_string(const CameraRoiConfig& config, int indent)
{
    std::ostringstream oss;
    const int level0 = 0;
    const int level1 = 1;
    const int level2 = 2;
    const int level3 = 3;

    oss << "{\n";
    write_indent(oss, indent, level1);
    oss << "\"camera_id\": \"" << escape_json_string(config.camera_id) << "\",\n";
    write_indent(oss, indent, level1);
    oss << "\"image_width\": " << config.image_width << ",\n";
    write_indent(oss, indent, level1);
    oss << "\"image_height\": " << config.image_height << ",\n";
    write_indent(oss, indent, level1);
    oss << "\"allowed_zones\": [";

    if (!config.allowed_zones.empty()) {
        oss << "\n";
    }

    for (std::size_t i = 0; i < config.allowed_zones.size(); ++i) {
        const auto& zone = config.allowed_zones[i];
        write_indent(oss, indent, level2);
        oss << "{\n";

        write_indent(oss, indent, level3);
        oss << "\"id\": \"" << escape_json_string(zone.id) << "\",\n";
        write_indent(oss, indent, level3);
        oss << "\"name\": \"" << escape_json_string(zone.name) << "\",\n";
        write_indent(oss, indent, level3);
        oss << "\"enabled\": " << (zone.enabled ? "true" : "false") << ",\n";
        write_indent(oss, indent, level3);
        oss << "\"points\": [";

        for (std::size_t p = 0; p < zone.points.size(); ++p) {
            if (p == 0) {
                oss << ' ';
            }
            oss << '[' << zone.points[p].x << ", " << zone.points[p].y << ']';
            if (p + 1 < zone.points.size()) {
                oss << ", ";
            } else {
                oss << ' ';
            }
        }

        oss << "]\n";
        write_indent(oss, indent, level2);
        oss << '}';

        if (i + 1 < config.allowed_zones.size()) {
            oss << ',';
        }
        oss << "\n";
    }

    if (!config.allowed_zones.empty()) {
        write_indent(oss, indent, level1);
    }
    oss << "]\n";
    write_indent(oss, indent, level0);
    oss << '}';

    return oss.str();
}

bool RoiRepository::save_to_file(const CameraRoiConfig& config, const std::string& path)
{
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << to_json_string(config) << '\n';
    return ofs.good();
}

} // namespace catcheye::guard::roi
