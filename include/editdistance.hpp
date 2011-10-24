#include <string>
#include <iostream>
#include <assert.h>

int
edit_distance(std::string const& lhs, std::string const& rhs) {
    const int n = lhs.size();
    const int m = rhs.size();
    int *dp[2] = { (int*)malloc((m+1)*sizeof(int)), (int*)malloc((m+1)*sizeof(int)) };
    dp[0] = dp[0] + 1;
    dp[1] = dp[1] + 1;
    dp[0][-1] = 0;
    for (int i = 0; i < m; ++i) {
        dp[0][i] = i + 1;
    }

    for (int i = 0; i < n; ++i) {
        dp[1][-1] = i+1;
        for (int j = 0; j < m; ++j) {
            if (lhs[i] == rhs[j]) {
                dp[1][j] = dp[0][j-1];
            }
            else {
                int m1 = dp[0][j] + 1;
                int m2 = dp[1][j-1] + 1;
                int m3 = dp[0][j-1] + 1;
                m1 = m1 < m2 ? m1 : m2;
                m1 = m1 < m3 ? m1 : m3;
                dp[1][j] = m1;
            }
        }
        int *tmp = dp[0];
        dp[0] = dp[1];
        dp[1] = tmp;
    }
    int ret = dp[0][m-1];
    free(dp[0] - 1);
    free(dp[1] - 1);
    return ret;
}

namespace editdistance {
    int
    test() {
        std::string s1, s2;
        s1 = "duckduckgo"; s2 = "duckduckgoose";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 3);

        s1 = "duckduckgoose"; s2 = "duckduckgo";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 3);

        s1 = "duckduckgo.com"; s2 = "duckduckgoose";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 4);

        s1 = "duckduckgoose"; s2 = "duckduckgo.com";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 4);

        s1 = "duckduckgo.com"; s2 = "dukgo.com";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 5);

        s1 = "luck"; s2 = "duck";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 1);

        s1 = "deer"; s2 = "duck";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 3);

        s1 = "elephant"; s2 = "duck";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 8);

        s1 = "duck"; s2 = "elephant";
        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<edit_distance(s1, s2)<<endl;
        assert(edit_distance(s1, s2) == 8);

        return 0;
    }
}
