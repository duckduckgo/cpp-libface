#include <string>
#include <iostream>
#include <assert.h>
#include <utility>
#include <time.h>

int
edit_distance(std::string const& lhs, std::string const& rhs) {
    const int n = lhs.size();
    const int m = rhs.size();
    int *mem = (int*)malloc((m + m + 2) * sizeof(int));
    int *dp0 = mem+1, *dp1 = mem+m+2;
    dp0[-1] = 0;
    for (int i = 0; i < m; ++i) {
        dp0[i] = i + 1;
    }

    for (int i = 0; i < n; ++i) {
        dp1[-1] = i+1;
        for (int j = 0; j < m; ++j) {
            if (lhs[i] == rhs[j]) {
                dp1[j] = dp0[j-1];
            }
            else {
                int m1 = dp0[j] + 1;
                int m2 = dp1[j-1] + 1;
                int m3 = dp0[j-1] + 1;
                m1 = m1 < m2 ? m1 : m2;
                m1 = m1 < m3 ? m1 : m3;
                dp1[j] = m1;
            }
        }
        std::swap(dp0, dp1);
    }
    int ret = dp0[m-1];
    free(mem);
    return ret;
}

namespace editdistance {
    int
    test() {
        printf("Testing edit_distance implementation\n");
        printf("------------------------------------\n");

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

        s1 = "abracadabra magic! magic! magic!"; s2 = "the great Hoodini";
        int x = 0;
        clock_t start = clock();
        const int ntimes = 200000;
        for (int i = 0; i < ntimes; ++i) {
            x = edit_distance(s1, s2);
        }
        clock_t end = clock();
        double sec_diff = (double)(end - start) / CLOCKS_PER_SEC;

        cerr<<"Edit Distance("<<s1<<", "<<s2<<"): "<<x<<endl;
        cerr<<"Time to compute "<<ntimes<<" times: "<<sec_diff
	    <<" second. sec/compare: "<<sec_diff/ntimes
	    <<" compare/sec: "<<ntimes/sec_diff<<endl;

	printf("\n");
        return 0;
    }
}
