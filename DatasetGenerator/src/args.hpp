#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

template <size_t N>
struct FixedString {
	char data[N]{};
	constexpr FixedString(const char (&str)[N]) {
		std::copy_n(str, N, data);
	}
	constexpr operator std::string_view() const {
		return std::string_view(data, N - 1);
	}
};

template<typename T, FixedString Name, FixedString Description, bool Required>
struct Option {
	using Type = T;
	static constexpr auto name = Name;
	static constexpr auto description = Description;
	static constexpr bool required = Required;
	static Type defaultValue() {
		return Type{};
	}
};

template<typename T, FixedString Name, FixedString Description, bool Required, T DefaultValue>
struct OptionD : Option<T, Name, Description, Required> {
	using Type = T;
	static constexpr Type defaultValue() {
		return DefaultValue;
	}
};

template <typename Tag>
class OptionValue {
public:
	using T = typename Tag::Type;
	T value = Tag::defaultValue();
};

template <typename... Tags>
class OptionRegistry : private OptionValue<Tags>... {
public:
	template <typename Tag>
	const typename Tag::Type& get() const {
		return OptionValue<Tag>::value;
	}

	template <typename Tag>
	void set(const typename Tag::Type& value) {
		OptionValue<Tag>::value = value;
	}

	bool parse(std::string_view name, const std::string& value) {
		return (parseOption<Tags>(name, value) || ...);
	}

	void help() {
		size_t maxOptionLength = 0;
		forEach([&maxOptionLength](const auto& tag, const auto&) {
			size_t optionLength = static_cast<size_t>(std::string_view(tag.name).size());
			if (optionLength > maxOptionLength) {
				maxOptionLength = optionLength;
			}
		});

		maxOptionLength += 3;
		std::cout << std::left << std::setw(maxOptionLength) << "Option" << std::setw(11) << "Default" << std::setw(11) << "Required" << "Description\n";
		forEach([maxOptionLength](const auto& tag, const auto& value) {
			std::cout << std::left << std::setw(maxOptionLength) << ("--" + std::string(tag.name)) << std::setw(11) 
				<< tag.defaultValue() << std::setw(11) << (tag.required ? "Yes" : "No") << std::string_view(tag.description) << '\n';
		});
	}

	bool validate() {
		bool valid = true;
		forEach([&valid](const auto& tag, const auto& value) {
			if (tag.required && value == tag.defaultValue()) {
				std::cout << "Missing required option: --" << std::string_view(tag.name) << "\n";
				valid = false;
			}
		});
		return valid;
	}

	bool parseArguments(int argc, char** argv) {
		for (int index = 1; index < argc;) {
			const std::string name = argv[index++];
			if (index >= argc || !name.starts_with("--")) {
				std::cout << "Invalid option: " << name << "\n";
				return false;
			}
			const std::string value = argv[index++];

			if (!parse(name.substr(2), value)) {
				std::cout << "Unknown option or invalid value: " << name << " " << value << "\n";
				return false;
			}
		}
		return validate();
	}

private:

	bool insEqual(std::string_view a, std::string_view b) {
		return std::equal(a.begin(), a.end(), b.begin(), b.end(),
			[](char ac, char bc) { return std::tolower(ac) == std::tolower(bc); });
	}

	template <typename Tag>
	bool parseOption(std::string_view name, const std::string& value) {
		if (name == Tag::name) {
			using Type = typename Tag::Type;
			if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, std::filesystem::path>) {
				OptionValue<Tag>::value = value;
				return true;
			} else if constexpr (std::is_same_v<Type, bool>) {
				if (insEqual(value, "true")) {
					OptionValue<Tag>::value = true;
					return true;
				} else if (insEqual(value, "false")) {
					OptionValue<Tag>::value = false;
					return true;
				}
			} 
			std::istringstream stream(value);
			typename Tag::Type parsed;
			stream >> parsed;
			if (!stream || !stream.eof()) {
				return false;
			}
			OptionValue<Tag>::value = parsed;
			return true;
		}
		return false;
	}

	template <typename F>
	void forEach(F&& func) {
		(func(Tags{}, this->template get<Tags>()), ...);
	}
};
