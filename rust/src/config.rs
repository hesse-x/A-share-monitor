use std::{
    collections::BTreeSet,
    fmt,
    io::{self, BufRead, BufReader, Read},
    time::Duration,
};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Config {
    pub codes: Vec<String>,
    pub frequency: Duration,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            codes: vec!["sh000001".into()],
            frequency: Duration::from_secs(60),
        }
    }
}

#[derive(Debug)]
pub struct ConfigError {
    line: usize,
    message: String,
}

impl fmt::Display for ConfigError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "line {}: {}", self.line, self.message)
    }
}

impl Config {
    pub fn parse(reader: impl Read) -> Result<Self, ConfigError> {
        enum Section {
            None,
            Codes,
            Frequency,
        }

        let mut section = Section::None;
        let mut codes = BTreeSet::new();
        let mut frequency = Duration::from_secs(60);

        for (index, line) in BufReader::new(reader).lines().enumerate() {
            let line_number = index + 1;
            let line = line.map_err(|error| ConfigError {
                line: line_number,
                message: error.to_string(),
            })?;
            let value = line.trim();
            if value.is_empty() || value.starts_with('#') {
                continue;
            }
            match value {
                "code:" => section = Section::Codes,
                "freq:" => section = Section::Frequency,
                _ => match section {
                    Section::Codes => {
                        validate_code(value).map_err(|message| ConfigError {
                            line: line_number,
                            message,
                        })?;
                        codes.insert(value.to_owned());
                    }
                    Section::Frequency => {
                        frequency = parse_duration(value).ok_or_else(|| ConfigError {
                            line: line_number,
                            message: "expected a duration such as 500ms, 10s, or 1m".into(),
                        })?;
                        section = Section::None;
                    }
                    Section::None => {
                        return Err(ConfigError {
                            line: line_number,
                            message: "expected `code:` or `freq:`".into(),
                        });
                    }
                },
            }
        }

        if matches!(section, Section::Frequency) {
            return Err(ConfigError {
                line: 0,
                message: "missing value after `freq:`".into(),
            });
        }
        if codes.is_empty() {
            codes.insert("sh000001".into());
        }
        Ok(Self {
            codes: codes.into_iter().collect(),
            frequency,
        })
    }
}

fn parse_duration(value: &str) -> Option<Duration> {
    let digit_count = value.bytes().take_while(u8::is_ascii_digit).count();
    if digit_count == 0 || digit_count == value.len() {
        return None;
    }
    let amount = value[..digit_count].parse::<u64>().ok()?;
    match &value[digit_count..] {
        "ms" => Some(Duration::from_millis(amount)),
        "s" => Some(Duration::from_secs(amount)),
        "m" => Some(Duration::from_secs(amount.checked_mul(60)?)),
        _ => None,
    }
}

pub fn validate_code(code: &str) -> Result<(), String> {
    if code.starts_with("test") {
        return Ok(());
    }
    if let Some(number) = code.strip_prefix("sh").or_else(|| code.strip_prefix("sz")) {
        if number.len() == 6 && number.bytes().all(|byte| byte.is_ascii_digit()) {
            return Ok(());
        }
        return Err("stock code must look like sh600000 or sz000001".into());
    }
    let Some((name, kind)) = code.split_once('-') else {
        return Err("unsupported code; expected a stock or index-future spread".into());
    };
    if matches!(name, "IH" | "IF" | "IC" | "IM") && matches!(kind, "Front" | "Next") {
        Ok(())
    } else {
        Err("future code must look like IH-Front or IM-Next".into())
    }
}

impl From<io::Error> for ConfigError {
    fn from(error: io::Error) -> Self {
        Self {
            line: 0,
            message: error.to_string(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_and_deduplicates_existing_format() {
        let input = b"code:\n sh000001\n sh000001\n IF-Next\nfreq:\n 5s\n";
        let config = Config::parse(&input[..]).unwrap();
        assert_eq!(config.codes, ["IF-Next", "sh000001"]);
        assert_eq!(config.frequency, Duration::from_secs(5));
    }

    #[test]
    fn rejects_bad_codes_and_durations() {
        assert!(Config::parse(&b"code:\nsh123\n"[..]).is_err());
        assert!(Config::parse(&b"freq:\n60\n"[..]).is_err());
    }
}
