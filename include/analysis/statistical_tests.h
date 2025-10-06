// include/analysis/statistical_tests.h
#pragma once

#include <vector>
#include <map>
#include <string>

namespace sentio::analysis {

/**
 * @brief Statistical testing utilities for strategy validation
 * 
 * Provides various statistical tests for analyzing strategy performance,
 * comparing strategies, and validating statistical significance of results.
 */
class StatisticalTests {
public:
    /**
     * @brief Result of a statistical test
     */
    struct TestResult {
        double statistic;           // Test statistic value
        double p_value;             // P-value
        bool significant;           // Whether result is statistically significant (p < 0.05)
        std::string test_name;      // Name of the test
        std::string interpretation; // Human-readable interpretation
    };
    
    /**
     * @brief Perform t-test to compare two samples
     * @param sample1 First sample
     * @param sample2 Second sample
     * @param paired Whether samples are paired (default: false)
     * @return TestResult with t-statistic and p-value
     */
    static TestResult t_test(
        const std::vector<double>& sample1,
        const std::vector<double>& sample2,
        bool paired = false
    );
    
    /**
     * @brief Perform one-sample t-test against a hypothesized mean
     * @param sample Sample data
     * @param hypothesized_mean Mean to test against
     * @return TestResult with t-statistic and p-value
     */
    static TestResult one_sample_t_test(
        const std::vector<double>& sample,
        double hypothesized_mean = 0.0
    );
    
    /**
     * @brief Perform chi-square goodness of fit test
     * @param observed Observed frequencies
     * @param expected Expected frequencies
     * @return TestResult with chi-square statistic and p-value
     */
    static TestResult chi_square_test(
        const std::vector<int>& observed,
        const std::vector<int>& expected
    );
    
    /**
     * @brief Perform Kolmogorov-Smirnov test for normality
     * @param sample Sample to test
     * @return TestResult indicating if sample is normally distributed
     */
    static TestResult ks_test_normality(const std::vector<double>& sample);
    
    /**
     * @brief Perform Jarque-Bera test for normality
     * @param sample Sample to test
     * @return TestResult indicating if sample is normally distributed
     */
    static TestResult jarque_bera_test(const std::vector<double>& sample);
    
    /**
     * @brief Perform Mann-Whitney U test (non-parametric alternative to t-test)
     * @param sample1 First sample
     * @param sample2 Second sample
     * @return TestResult with U-statistic and p-value
     */
    static TestResult mann_whitney_test(
        const std::vector<double>& sample1,
        const std::vector<double>& sample2
    );
    
    /**
     * @brief Perform Wilcoxon signed-rank test (non-parametric paired test)
     * @param sample1 First sample
     * @param sample2 Second sample
     * @return TestResult with W-statistic and p-value
     */
    static TestResult wilcoxon_test(
        const std::vector<double>& sample1,
        const std::vector<double>& sample2
    );
    
    /**
     * @brief Calculate correlation between two series
     * @param x First series
     * @param y Second series
     * @return Pearson correlation coefficient
     */
    static double correlation(
        const std::vector<double>& x,
        const std::vector<double>& y
    );
    
    /**
     * @brief Calculate Spearman rank correlation
     * @param x First series
     * @param y Second series
     * @return Spearman correlation coefficient
     */
    static double spearman_correlation(
        const std::vector<double>& x,
        const std::vector<double>& y
    );
    
    /**
     * @brief Perform autocorrelation test
     * @param series Time series data
     * @param lag Lag to test (default: 1)
     * @return Autocorrelation coefficient
     */
    static double autocorrelation(
        const std::vector<double>& series,
        int lag = 1
    );
    
    /**
     * @brief Perform Durbin-Watson test for autocorrelation
     * @param residuals Residuals from regression
     * @return TestResult with Durbin-Watson statistic
     */
    static TestResult durbin_watson_test(const std::vector<double>& residuals);
    
    /**
     * @brief Calculate confidence interval for mean
     * @param sample Sample data
     * @param confidence_level Confidence level (default: 0.95)
     * @return Pair of (lower_bound, upper_bound)
     */
    static std::pair<double, double> confidence_interval(
        const std::vector<double>& sample,
        double confidence_level = 0.95
    );
    
    /**
     * @brief Perform bootstrap resampling
     * @param sample Original sample
     * @param num_resamples Number of bootstrap samples
     * @return Vector of bootstrap sample means
     */
    static std::vector<double> bootstrap(
        const std::vector<double>& sample,
        int num_resamples = 1000
    );
    
    /**
     * @brief Calculate percentile from sample
     * @param sample Sample data
     * @param percentile Percentile to calculate (0-100)
     * @return Value at given percentile
     */
    static double percentile(
        const std::vector<double>& sample,
        double percentile
    );

private:
    /**
     * @brief Calculate t-distribution cumulative probability
     */
    static double t_distribution_cdf(double t, int df);
    
    /**
     * @brief Calculate chi-square distribution cumulative probability
     */
    static double chi_square_cdf(double chi_square, int df);
    
    /**
     * @brief Calculate normal distribution cumulative probability
     */
    static double normal_cdf(double z);
    
    /**
     * @brief Calculate standard error of mean
     */
    static double standard_error(const std::vector<double>& sample);
    
    /**
     * @brief Calculate skewness of sample
     */
    static double skewness(const std::vector<double>& sample);
    
    /**
     * @brief Calculate kurtosis of sample
     */
    static double kurtosis(const std::vector<double>& sample);
};

/**
 * @brief Helper class for cross-validation analysis
 */
class CrossValidation {
public:
    /**
     * @brief Split data into k folds
     * @param data_size Size of dataset
     * @param k Number of folds
     * @return Vector of (train_indices, test_indices) pairs
     */
    static std::vector<std::pair<std::vector<int>, std::vector<int>>> 
    k_fold_split(int data_size, int k);
    
    /**
     * @brief Time-series split (respecting temporal order)
     * @param data_size Size of dataset
     * @param n_splits Number of splits
     * @return Vector of (train_indices, test_indices) pairs
     */
    static std::vector<std::pair<std::vector<int>, std::vector<int>>>
    time_series_split(int data_size, int n_splits);
};

/**
 * @brief Helper class for multiple comparison corrections
 */
class MultipleComparisonCorrection {
public:
    /**
     * @brief Apply Bonferroni correction to p-values
     * @param p_values Vector of p-values
     * @return Corrected p-values
     */
    static std::vector<double> bonferroni(const std::vector<double>& p_values);
    
    /**
     * @brief Apply Holm-Bonferroni correction
     * @param p_values Vector of p-values
     * @return Corrected p-values
     */
    static std::vector<double> holm_bonferroni(const std::vector<double>& p_values);
    
    /**
     * @brief Apply Benjamini-Hochberg FDR correction
     * @param p_values Vector of p-values
     * @param fdr False discovery rate (default: 0.05)
     * @return Corrected p-values
     */
    static std::vector<double> benjamini_hochberg(
        const std::vector<double>& p_values,
        double fdr = 0.05
    );
};

} // namespace sentio::analysis


