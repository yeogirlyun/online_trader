#include "analysis/statistical_tests.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <limits>

namespace sentio::analysis {

// StatisticalTests implementation

StatisticalTests::TestResult StatisticalTests::t_test(
    const std::vector<double>& sample1,
    const std::vector<double>& sample2,
    bool paired
) {
    TestResult result;
    result.test_name = paired ? "Paired t-test" : "Independent t-test";
    
    if (sample1.empty() || sample2.empty()) {
        throw std::invalid_argument("Samples cannot be empty");
    }
    
    if (paired && sample1.size() != sample2.size()) {
        throw std::invalid_argument("Paired samples must have equal size");
    }
    
    if (paired) {
        // Paired t-test
        std::vector<double> differences;
        for (size_t i = 0; i < sample1.size(); ++i) {
            differences.push_back(sample1[i] - sample2[i]);
        }
        return one_sample_t_test(differences, 0.0);
    } else {
        // Independent t-test
        double mean1 = std::accumulate(sample1.begin(), sample1.end(), 0.0) / sample1.size();
        double mean2 = std::accumulate(sample2.begin(), sample2.end(), 0.0) / sample2.size();
        
        // Calculate variances
        double var1 = 0.0, var2 = 0.0;
        for (const auto& x : sample1) var1 += (x - mean1) * (x - mean1);
        for (const auto& x : sample2) var2 += (x - mean2) * (x - mean2);
        var1 /= (sample1.size() - 1);
        var2 /= (sample2.size() - 1);
        
        // Pooled standard error
        double se = std::sqrt(var1 / sample1.size() + var2 / sample2.size());
        
        // t-statistic
        result.statistic = (mean1 - mean2) / se;
        
        // Degrees of freedom (Welch's approximation)
        int df = static_cast<int>(sample1.size() + sample2.size() - 2);
        
        // Calculate p-value (two-tailed)
        result.p_value = 2.0 * (1.0 - t_distribution_cdf(std::abs(result.statistic), df));
        result.significant = result.p_value < 0.05;
        
        result.interpretation = result.significant ? 
            "Samples have significantly different means" : 
            "No significant difference between sample means";
    }
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::one_sample_t_test(
    const std::vector<double>& sample,
    double hypothesized_mean
) {
    TestResult result;
    result.test_name = "One-sample t-test";
    
    if (sample.empty()) {
        throw std::invalid_argument("Sample cannot be empty");
    }
    
    double mean = std::accumulate(sample.begin(), sample.end(), 0.0) / sample.size();
    
    // Calculate variance
    double variance = 0.0;
    for (const auto& x : sample) {
        variance += (x - mean) * (x - mean);
    }
    variance /= (sample.size() - 1);
    
    // Calculate t-statistic
    double se = std::sqrt(variance / sample.size());
    result.statistic = (mean - hypothesized_mean) / se;
    
    // Degrees of freedom
    int df = static_cast<int>(sample.size() - 1);
    
    // Calculate p-value (two-tailed)
    result.p_value = 2.0 * (1.0 - t_distribution_cdf(std::abs(result.statistic), df));
    result.significant = result.p_value < 0.05;
    
    result.interpretation = result.significant ?
        "Sample mean significantly differs from hypothesized mean" :
        "Sample mean does not significantly differ from hypothesized mean";
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::chi_square_test(
    const std::vector<int>& observed,
    const std::vector<int>& expected
) {
    TestResult result;
    result.test_name = "Chi-square goodness of fit test";
    
    if (observed.size() != expected.size()) {
        throw std::invalid_argument("Observed and expected must have same size");
    }
    
    result.statistic = 0.0;
    for (size_t i = 0; i < observed.size(); ++i) {
        if (expected[i] == 0) continue;
        double diff = observed[i] - expected[i];
        result.statistic += (diff * diff) / expected[i];
    }
    
    int df = static_cast<int>(observed.size() - 1);
    result.p_value = 1.0 - chi_square_cdf(result.statistic, df);
    result.significant = result.p_value < 0.05;
    
    result.interpretation = result.significant ?
        "Observed distribution significantly differs from expected" :
        "Observed distribution fits expected distribution";
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::ks_test_normality(
    const std::vector<double>& sample
) {
    TestResult result;
    result.test_name = "Kolmogorov-Smirnov normality test";
    
    // TODO: Implement full KS test
    // This is a stub - implement based on your requirements
    
    result.statistic = 0.0;
    result.p_value = 1.0;
    result.significant = false;
    result.interpretation = "KS test not fully implemented - stub only";
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::jarque_bera_test(
    const std::vector<double>& sample
) {
    TestResult result;
    result.test_name = "Jarque-Bera normality test";
    
    if (sample.size() < 4) {
        throw std::invalid_argument("Sample size must be at least 4");
    }
    
    double s = skewness(sample);
    double k = kurtosis(sample);
    
    int n = static_cast<int>(sample.size());
    result.statistic = (n / 6.0) * (s * s + 0.25 * (k - 3) * (k - 3));
    
    // Chi-square with 2 degrees of freedom
    result.p_value = 1.0 - chi_square_cdf(result.statistic, 2);
    result.significant = result.p_value < 0.05;
    
    result.interpretation = result.significant ?
        "Sample significantly deviates from normal distribution" :
        "Sample is consistent with normal distribution";
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::mann_whitney_test(
    const std::vector<double>& sample1,
    const std::vector<double>& sample2
) {
    TestResult result;
    result.test_name = "Mann-Whitney U test";
    
    // TODO: Implement Mann-Whitney U test
    // This is a stub - implement based on your requirements
    
    result.statistic = 0.0;
    result.p_value = 1.0;
    result.significant = false;
    result.interpretation = "Mann-Whitney test not fully implemented - stub only";
    
    return result;
}

StatisticalTests::TestResult StatisticalTests::wilcoxon_test(
    const std::vector<double>& sample1,
    const std::vector<double>& sample2
) {
    TestResult result;
    result.test_name = "Wilcoxon signed-rank test";
    
    // TODO: Implement Wilcoxon test
    // This is a stub - implement based on your requirements
    
    result.statistic = 0.0;
    result.p_value = 1.0;
    result.significant = false;
    result.interpretation = "Wilcoxon test not fully implemented - stub only";
    
    return result;
}

double StatisticalTests::correlation(
    const std::vector<double>& x,
    const std::vector<double>& y
) {
    if (x.size() != y.size() || x.empty()) {
        throw std::invalid_argument("Vectors must be non-empty and same size");
    }
    
    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
    
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        sum_xy += dx * dy;
        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
    }
    
    if (sum_x2 == 0.0 || sum_y2 == 0.0) return 0.0;
    
    return sum_xy / std::sqrt(sum_x2 * sum_y2);
}

double StatisticalTests::spearman_correlation(
    const std::vector<double>& x,
    const std::vector<double>& y
) {
    // TODO: Implement Spearman correlation with ranking
    // This is a stub - for now return Pearson correlation
    return correlation(x, y);
}

double StatisticalTests::autocorrelation(
    const std::vector<double>& series,
    int lag
) {
    if (series.size() <= static_cast<size_t>(lag)) {
        throw std::invalid_argument("Lag must be less than series length");
    }
    
    double mean = std::accumulate(series.begin(), series.end(), 0.0) / series.size();
    
    double numerator = 0.0, denominator = 0.0;
    
    for (size_t i = lag; i < series.size(); ++i) {
        numerator += (series[i] - mean) * (series[i - lag] - mean);
    }
    
    for (const auto& value : series) {
        denominator += (value - mean) * (value - mean);
    }
    
    return (denominator > 0) ? (numerator / denominator) : 0.0;
}

StatisticalTests::TestResult StatisticalTests::durbin_watson_test(
    const std::vector<double>& residuals
) {
    TestResult result;
    result.test_name = "Durbin-Watson test";
    
    if (residuals.size() < 2) {
        throw std::invalid_argument("Need at least 2 residuals");
    }
    
    double sum_squared_diff = 0.0;
    double sum_squared_resid = 0.0;
    
    for (size_t i = 1; i < residuals.size(); ++i) {
        double diff = residuals[i] - residuals[i-1];
        sum_squared_diff += diff * diff;
    }
    
    for (const auto& r : residuals) {
        sum_squared_resid += r * r;
    }
    
    result.statistic = sum_squared_resid > 0 ? 
        (sum_squared_diff / sum_squared_resid) : 0.0;
    
    // DW statistic ranges from 0 to 4
    // ~2 indicates no autocorrelation
    // <2 indicates positive autocorrelation
    // >2 indicates negative autocorrelation
    
    if (result.statistic < 1.5 || result.statistic > 2.5) {
        result.significant = true;
        result.interpretation = "Significant autocorrelation detected";
    } else {
        result.significant = false;
        result.interpretation = "No significant autocorrelation";
    }
    
    result.p_value = 0.0;  // Exact p-value calculation is complex
    
    return result;
}

std::pair<double, double> StatisticalTests::confidence_interval(
    const std::vector<double>& sample,
    double confidence_level
) {
    if (sample.empty()) {
        throw std::invalid_argument("Sample cannot be empty");
    }
    
    double mean = std::accumulate(sample.begin(), sample.end(), 0.0) / sample.size();
    double se = standard_error(sample);
    
    // For 95% CI, use t-value of approximately 1.96 (for large samples)
    // For small samples, should use t-distribution
    double t_value = 1.96;  // Simplified
    
    if (confidence_level == 0.99) t_value = 2.576;
    else if (confidence_level == 0.90) t_value = 1.645;
    
    double margin = t_value * se;
    
    return {mean - margin, mean + margin};
}

std::vector<double> StatisticalTests::bootstrap(
    const std::vector<double>& sample,
    int num_resamples
) {
    std::vector<double> bootstrap_means;
    bootstrap_means.reserve(num_resamples);
    
    for (int i = 0; i < num_resamples; ++i) {
        std::vector<double> resample;
        resample.reserve(sample.size());
        
        // Random sampling with replacement
        for (size_t j = 0; j < sample.size(); ++j) {
            int idx = rand() % sample.size();
            resample.push_back(sample[idx]);
        }
        
        double mean = std::accumulate(resample.begin(), resample.end(), 0.0) / resample.size();
        bootstrap_means.push_back(mean);
    }
    
    return bootstrap_means;
}

double StatisticalTests::percentile(
    const std::vector<double>& sample,
    double percentile
) {
    if (sample.empty()) {
        throw std::invalid_argument("Sample cannot be empty");
    }
    
    std::vector<double> sorted = sample;
    std::sort(sorted.begin(), sorted.end());
    
    double index = (percentile / 100.0) * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));
    
    if (lower == upper) {
        return sorted[lower];
    }
    
    double weight = index - lower;
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

// Private helper methods

double StatisticalTests::t_distribution_cdf(double t, int /*df*/) {
    // Simplified approximation
    // For production, use a proper t-distribution CDF implementation
    return normal_cdf(t);  // Approximation for large df
}

namespace {
// Regularized lower incomplete gamma P(a, x) using series/continued fraction (Lentz)
static double regularized_gamma_p(double a, double x) {
    if (a <= 0.0 || x < 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (x == 0.0) return 0.0;
    const double eps = 1e-12;
    const int max_iter = 1000;
    const double gln = std::lgamma(a);
    // Use series when x < a + 1
    if (x < a + 1.0) {
        double sum = 1.0 / a;
        double term = sum;
        double ap = a;
        for (int n = 1; n <= max_iter; ++n) {
            ap += 1.0;
            term *= x / ap;
            sum += term;
            if (std::fabs(term) < std::fabs(sum) * eps) break;
        }
        return sum * std::exp(-x + a * std::log(x) - gln);
    }
    // Otherwise use continued fraction for Q(a, x) then return 1 - Q
    double b = x + 1.0 - a;
    double c = 1.0 / std::numeric_limits<double>::min();
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= max_iter; ++i) {
        double an = -static_cast<double>(i) * (static_cast<double>(i) - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < std::numeric_limits<double>::min()) d = std::numeric_limits<double>::min();
        c = b + an / c;
        if (std::fabs(c) < std::numeric_limits<double>::min()) c = std::numeric_limits<double>::min();
        d = 1.0 / d;
        double delta = d * c;
        h *= delta;
        if (std::fabs(delta - 1.0) < eps) break;
    }
    double q = std::exp(-x + a * std::log(x) - gln) * h;
    return 1.0 - q;
}
}

double StatisticalTests::chi_square_cdf(double chi_square, int df) {
    if (chi_square < 0.0 || df <= 0) return 0.0;
    double k_over_2 = static_cast<double>(df) * 0.5;
    double x_over_2 = chi_square * 0.5;
    return regularized_gamma_p(k_over_2, x_over_2);
}

double StatisticalTests::normal_cdf(double z) {
    // Approximation using error function
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
}

double StatisticalTests::standard_error(const std::vector<double>& sample) {
    if (sample.size() < 2) return 0.0;
    
    double mean = std::accumulate(sample.begin(), sample.end(), 0.0) / sample.size();
    double variance = 0.0;
    
    for (const auto& x : sample) {
        variance += (x - mean) * (x - mean);
    }
    variance /= (sample.size() - 1);
    
    return std::sqrt(variance / sample.size());
}

double StatisticalTests::skewness(const std::vector<double>& sample) {
    if (sample.size() < 3) return 0.0;
    
    double mean = std::accumulate(sample.begin(), sample.end(), 0.0) / sample.size();
    
    double m2 = 0.0, m3 = 0.0;
    for (const auto& x : sample) {
        double diff = x - mean;
        m2 += diff * diff;
        m3 += diff * diff * diff;
    }
    
    m2 /= sample.size();
    m3 /= sample.size();
    
    if (m2 == 0.0) return 0.0;
    
    return m3 / std::pow(m2, 1.5);
}

double StatisticalTests::kurtosis(const std::vector<double>& sample) {
    if (sample.size() < 4) return 0.0;
    
    double mean = std::accumulate(sample.begin(), sample.end(), 0.0) / sample.size();
    
    double m2 = 0.0, m4 = 0.0;
    for (const auto& x : sample) {
        double diff = x - mean;
        double diff2 = diff * diff;
        m2 += diff2;
        m4 += diff2 * diff2;
    }
    
    m2 /= sample.size();
    m4 /= sample.size();
    
    if (m2 == 0.0) return 0.0;
    
    return m4 / (m2 * m2);
}

// CrossValidation implementation

std::vector<std::pair<std::vector<int>, std::vector<int>>> 
CrossValidation::k_fold_split(int data_size, int k) {
    std::vector<std::pair<std::vector<int>, std::vector<int>>> splits;
    
    int fold_size = data_size / k;
    
    for (int fold = 0; fold < k; ++fold) {
        std::vector<int> train_indices, test_indices;
        
        int test_start = fold * fold_size;
        int test_end = (fold == k - 1) ? data_size : (fold + 1) * fold_size;
        
        for (int i = 0; i < data_size; ++i) {
            if (i >= test_start && i < test_end) {
                test_indices.push_back(i);
            } else {
                train_indices.push_back(i);
            }
        }
        
        splits.push_back({train_indices, test_indices});
    }
    
    return splits;
}

std::vector<std::pair<std::vector<int>, std::vector<int>>>
CrossValidation::time_series_split(int data_size, int n_splits) {
    std::vector<std::pair<std::vector<int>, std::vector<int>>> splits;
    
    int test_size = data_size / (n_splits + 1);
    
    for (int split = 0; split < n_splits; ++split) {
        std::vector<int> train_indices, test_indices;
        
        int train_end = (split + 1) * test_size;
        int test_end = std::min(train_end + test_size, data_size);
        
        for (int i = 0; i < train_end; ++i) {
            train_indices.push_back(i);
        }
        
        for (int i = train_end; i < test_end; ++i) {
            test_indices.push_back(i);
        }
        
        splits.push_back({train_indices, test_indices});
    }
    
    return splits;
}

// MultipleComparisonCorrection implementation

std::vector<double> MultipleComparisonCorrection::bonferroni(
    const std::vector<double>& p_values
) {
    std::vector<double> corrected;
    corrected.reserve(p_values.size());
    
    int m = static_cast<int>(p_values.size());
    for (const auto& p : p_values) {
        corrected.push_back(std::min(p * m, 1.0));
    }
    
    return corrected;
}

std::vector<double> MultipleComparisonCorrection::holm_bonferroni(
    const std::vector<double>& p_values
) {
    // TODO: Implement Holm-Bonferroni
    return bonferroni(p_values);  // Stub - use simple Bonferroni for now
}

std::vector<double> MultipleComparisonCorrection::benjamini_hochberg(
    const std::vector<double>& p_values,
    double /*fdr*/
) {
    // TODO: Implement Benjamini-Hochberg FDR
    return bonferroni(p_values);  // Stub - use simple Bonferroni for now
}

} // namespace sentio::analysis


